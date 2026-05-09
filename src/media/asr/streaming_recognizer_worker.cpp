#include "media/asr/streaming_recognizer_worker.h"

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include <QDir>
#include <QFileInfo>

extern "C" {
#include <sherpa-onnx/c-api/c-api.h>
}

namespace shatv::media::asr {

namespace {

constexpr int kAsrFeatureSampleRate = 16000;
constexpr int kAsrFeatureDim = 80;

QString ModelFilePath(const StreamingRecognizerConfig &config, const QString &file_name) {
    return QDir(config.model_dir).filePath(file_name);
}

bool RequireModelFile(const QString &path, QString *error_message) {
    if (!QFileInfo(path).isFile()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("required ASR model file is missing: %1").arg(path);
        }
        return false;
    }
    return true;
}

bool ValidateProvider(const QString &provider, QString *error_message) {
    if (provider == QStringLiteral("cpu") ||
        provider == QStringLiteral("cuda") ||
        provider == QStringLiteral("coreml")) {
        return true;
    }

    if (error_message != nullptr) {
        *error_message = QStringLiteral("unsupported ASR provider: %1").arg(provider);
    }
    return false;
}

}  // namespace

class StreamingRecognizerWorker::Impl final {
   public:
    ~Impl() {
        Stop();
    }

    bool StartSession(const StreamingRecognizerConfig &config, QString *error_message) {
        Stop();

        if (config.model_dir.isEmpty()) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("missing ASR model directory");
            }
            return false;
        }
        if (config.num_threads <= 0) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("ASR num_threads must be greater than zero");
            }
            return false;
        }
        if (config.max_queued_chunks <= 0) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("ASR max_queued_chunks must be greater than zero");
            }
            return false;
        }
        if (!ValidateProvider(config.provider, error_message)) {
            return false;
        }

        encoder_path_ = ModelFilePath(config, config.encoder_name);
        decoder_path_ = ModelFilePath(config, config.decoder_name);
        tokens_path_ = ModelFilePath(config, config.tokens_name);
        if (!RequireModelFile(encoder_path_, error_message) ||
            !RequireModelFile(decoder_path_, error_message) ||
            !RequireModelFile(tokens_path_, error_message)) {
            return false;
        }

        encoder_path_text_ = encoder_path_.toStdString();
        decoder_path_text_ = decoder_path_.toStdString();
        tokens_path_text_ = tokens_path_.toStdString();
        provider_text_ = config.provider.toStdString();
        max_queued_chunks_ = config.max_queued_chunks;
        result_callback_ = config.result_callback;
        last_text_.clear();

        SherpaOnnxOnlineRecognizerConfig recognizer_config{};
        recognizer_config.feat_config.sample_rate = kAsrFeatureSampleRate;
        recognizer_config.feat_config.feature_dim = kAsrFeatureDim;
        recognizer_config.model_config.paraformer.encoder = encoder_path_text_.c_str();
        recognizer_config.model_config.paraformer.decoder = decoder_path_text_.c_str();
        recognizer_config.model_config.tokens = tokens_path_text_.c_str();
        recognizer_config.model_config.provider = provider_text_.c_str();
        recognizer_config.model_config.num_threads = config.num_threads;
        recognizer_config.model_config.modeling_unit = "cjkchar";
        recognizer_config.decoding_method = "greedy_search";
        recognizer_config.max_active_paths = 4;
        recognizer_config.enable_endpoint = 1;
        recognizer_config.rule1_min_trailing_silence = 2.4F;
        recognizer_config.rule2_min_trailing_silence = 1.2F;
        recognizer_config.rule3_min_utterance_length = 20.0F;
        recognizer_config.hotwords_score = 1.5F;

        recognizer_.reset(SherpaOnnxCreateOnlineRecognizer(&recognizer_config));
        if (!recognizer_) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("failed to create sherpa-onnx online recognizer");
            }
            return false;
        }

        stream_.reset(SherpaOnnxCreateOnlineStream(recognizer_.get()));
        if (!stream_) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("failed to create sherpa-onnx online stream");
            }
            recognizer_.reset();
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_requested_ = false;
            input_finished_ = false;
            worker_error_.clear();
            queue_.clear();
            running_ = true;
        }

        worker_thread_ = std::thread([this]() { Run(); });
        return true;
    }

    bool Enqueue(PcmChunk chunk, QString *error_message) {
        if (chunk.samples.empty()) {
            return true;
        }
        if (chunk.sample_rate <= 0 || chunk.channel_count != 1) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("invalid ASR PCM chunk");
            }
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_ || stop_requested_ || input_finished_) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("ASR worker is not running");
            }
            return false;
        }
        if (queue_.size() >= static_cast<std::size_t>(max_queued_chunks_)) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("ASR PCM queue overflow: %1 queued chunks")
                                     .arg(static_cast<int>(queue_.size()));
            }
            return false;
        }

        queue_.push_back(std::move(chunk));
        condition_.notify_one();
        return true;
    }

    bool FinishSession(QString *error_message) {
        bool should_notify = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (running_) {
                input_finished_ = true;
                should_notify = true;
            }
        }
        if (should_notify) {
            condition_.notify_one();
        }
        JoinWorker();

        const QString worker_error = WorkerError();
        StopHandles();
        if (!worker_error.isEmpty()) {
            if (error_message != nullptr) {
                *error_message = worker_error;
            }
            return false;
        }
        return true;
    }

    void Stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_requested_ = true;
            queue_.clear();
        }
        condition_.notify_one();
        JoinWorker();
        StopHandles();
    }

    bool Running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

   private:
    struct RecognizerDeleter {
        void operator()(const SherpaOnnxOnlineRecognizer *recognizer) const {
            if (recognizer != nullptr) {
                SherpaOnnxDestroyOnlineRecognizer(recognizer);
            }
        }
    };

    struct StreamDeleter {
        void operator()(const SherpaOnnxOnlineStream *stream) const {
            if (stream != nullptr) {
                SherpaOnnxDestroyOnlineStream(stream);
            }
        }
    };

    struct ResultDeleter {
        void operator()(const SherpaOnnxOnlineRecognizerResult *result) const {
            if (result != nullptr) {
                SherpaOnnxDestroyOnlineRecognizerResult(result);
            }
        }
    };

    using RecognizerPtr = std::unique_ptr<const SherpaOnnxOnlineRecognizer, RecognizerDeleter>;
    using StreamPtr = std::unique_ptr<const SherpaOnnxOnlineStream, StreamDeleter>;
    using ResultPtr = std::unique_ptr<const SherpaOnnxOnlineRecognizerResult, ResultDeleter>;

    void Run() {
        while (true) {
            PcmChunk chunk;
            bool should_finish = false;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                condition_.wait(lock, [this]() {
                    return stop_requested_ || input_finished_ || !queue_.empty();
                });

                if (stop_requested_) {
                    running_ = false;
                    return;
                }
                if (queue_.empty() && input_finished_) {
                    should_finish = true;
                } else if (!queue_.empty()) {
                    chunk = std::move(queue_.front());
                    queue_.pop_front();
                }
            }

            if (should_finish) {
                DecodeFinal();
                std::lock_guard<std::mutex> lock(mutex_);
                running_ = false;
                return;
            }

            DecodeChunk(chunk);
        }
    }

    void DecodeChunk(const PcmChunk &chunk) {
        if (!recognizer_ || !stream_) {
            SetWorkerError(QStringLiteral("ASR recognizer session is not initialized"));
            return;
        }

        SherpaOnnxOnlineStreamAcceptWaveform(stream_.get(),
                                            chunk.sample_rate,
                                            chunk.samples.data(),
                                            static_cast<int32_t>(chunk.samples.size()));
        DecodeReady();
        EmitResult(false);
    }

    void DecodeFinal() {
        if (!recognizer_ || !stream_) {
            SetWorkerError(QStringLiteral("ASR recognizer session is not initialized"));
            return;
        }

        SherpaOnnxOnlineStreamSetOption(stream_.get(), "is_final", "1");
        SherpaOnnxOnlineStreamInputFinished(stream_.get());
        DecodeReady();
        EmitResult(true);
    }

    void DecodeReady() {
        while (SherpaOnnxIsOnlineStreamReady(recognizer_.get(), stream_.get()) != 0) {
            SherpaOnnxDecodeOnlineStream(recognizer_.get(), stream_.get());
        }
    }

    void EmitResult(bool is_final) {
        ResultPtr result(SherpaOnnxGetOnlineStreamResult(recognizer_.get(), stream_.get()));
        if (!result || result->text == nullptr || result->text[0] == '\0') {
            return;
        }

        const QString text = QString::fromUtf8(result->text);
        if (!is_final && text == last_text_) {
            return;
        }
        last_text_ = text;

        if (result_callback_) {
            result_callback_(StreamingRecognitionResult{
                .text = text,
                .is_final = is_final,
            });
        }
    }

    void SetWorkerError(QString error_message) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (worker_error_.isEmpty()) {
            worker_error_ = std::move(error_message);
        }
        stop_requested_ = true;
        running_ = false;
    }

    QString WorkerError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return worker_error_;
    }

    void JoinWorker() {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }

    void StopHandles() {
        stream_.reset();
        recognizer_.reset();
        result_callback_ = {};
        last_text_.clear();
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.clear();
        running_ = false;
        stop_requested_ = false;
        input_finished_ = false;
    }

    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<PcmChunk> queue_;
    std::thread worker_thread_;
    bool running_ = false;
    bool stop_requested_ = false;
    bool input_finished_ = false;
    int max_queued_chunks_ = 64;
    QString worker_error_;

    RecognizerPtr recognizer_;
    StreamPtr stream_;
    std::function<void(const StreamingRecognitionResult &result)> result_callback_;
    QString last_text_;
    QString encoder_path_;
    QString decoder_path_;
    QString tokens_path_;
    std::string encoder_path_text_;
    std::string decoder_path_text_;
    std::string tokens_path_text_;
    std::string provider_text_;
};

StreamingRecognizerWorker::StreamingRecognizerWorker() : impl_(std::make_unique<Impl>()) {}

StreamingRecognizerWorker::~StreamingRecognizerWorker() = default;

bool StreamingRecognizerWorker::StartSession(const StreamingRecognizerConfig &config, QString *error_message) {
    return impl_->StartSession(config, error_message);
}

bool StreamingRecognizerWorker::Enqueue(PcmChunk chunk, QString *error_message) {
    return impl_->Enqueue(std::move(chunk), error_message);
}

bool StreamingRecognizerWorker::FinishSession(QString *error_message) {
    return impl_->FinishSession(error_message);
}

void StreamingRecognizerWorker::Stop() {
    impl_->Stop();
}

bool StreamingRecognizerWorker::Running() const {
    return impl_->Running();
}

}  // namespace shatv::media::asr
