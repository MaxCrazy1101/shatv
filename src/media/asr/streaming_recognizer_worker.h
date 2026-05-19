#pragma once

#include <QString>
#include <QtGlobal>
#include <functional>
#include <memory>

#include "media/asr/pcm_converter.h"

namespace shatv::media::asr {

struct StreamingRecognitionResult {
    QString text;
    bool is_final = false;
    qint64 latency_ms = -1;
};

struct StreamingRecognizerConfig {
    QString model_dir;
    QString encoder_name = QStringLiteral("encoder.int8.onnx");
    QString decoder_name = QStringLiteral("decoder.int8.onnx");
    QString tokens_name = QStringLiteral("tokens.txt");
    QString provider = QStringLiteral("cpu");
    int num_threads = 1;
    int max_queued_chunks = 64;
    bool benchmark_logging = false;
    std::function<void(const StreamingRecognitionResult &result)> result_callback;
};

class StreamingRecognizerWorker final {
   public:
    StreamingRecognizerWorker();
    ~StreamingRecognizerWorker();

    StreamingRecognizerWorker(const StreamingRecognizerWorker &) = delete;
    StreamingRecognizerWorker &operator=(const StreamingRecognizerWorker &) = delete;

    bool StartSession(const StreamingRecognizerConfig &config, QString *error_message);
    bool Enqueue(PcmChunk chunk, QString *error_message);
    bool FinishSession(QString *error_message);
    void Stop();
    bool Running() const;

   private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace shatv::media::asr
