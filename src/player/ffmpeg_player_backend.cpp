#include "player/ffmpeg_player_backend.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>
#include <QtGlobal>

#include "app/asr_model_service.h"
#include "app/logging.h"
#include "domain/player_snapshot.h"
#include "media/decode/audio_decoder.h"
#include "media/decode/video_decoder.h"
#include "media/demux/demuxer.h"
#include "player/retry_schedule.h"

extern "C" {
#include <libavcodec/packet.h>
}

namespace shatv::player {

namespace {

constexpr qint64 kVideoEarlyToleranceUsecs = 25000;
constexpr qint64 kVideoLateDropUsecs = 100000;
constexpr unsigned long kVideoDrainSleepMillis = 5;
#if defined(SHATV_ENABLE_ASR)
constexpr int kDefaultAsrNumThreads = 1;
constexpr int kDefaultAsrMaxQueuedChunks = 64;
#endif

struct AvPacketDeleter {
    void operator()(AVPacket *packet) const {
        av_packet_free(&packet);
    }
};

using AvPacketPtr = std::unique_ptr<AVPacket, AvPacketDeleter>;

PlaybackPipelineResult PipelineFinished() {
    return PlaybackPipelineResult{
        .status = PlaybackPipelineStatus::kFinished,
        .error_message = {},
    };
}

PlaybackPipelineResult PipelineAborted() {
    return PlaybackPipelineResult{
        .status = PlaybackPipelineStatus::kAborted,
        .error_message = {},
    };
}

PlaybackPipelineResult PipelineFailed(QString error_message) {
    return PlaybackPipelineResult{
        .status = PlaybackPipelineStatus::kFailed,
        .error_message = std::move(error_message),
    };
}

bool ReconnectOnEndOfStream(domain::SourceKind source_kind) {
    return source_kind == domain::SourceKind::kPlaylistChannelLive;
}

QString SourceKindName(domain::SourceKind source_kind) {
    switch (source_kind) {
        case domain::SourceKind::kLocalFile:
            return QStringLiteral("local_file");
        case domain::SourceKind::kDirectRemoteMedia:
            return QStringLiteral("direct_remote_media");
        case domain::SourceKind::kRemotePlaylistFetch:
            return QStringLiteral("remote_playlist_fetch");
        case domain::SourceKind::kPlaylistChannelLive:
            return QStringLiteral("playlist_channel_live");
    }
    return QStringLiteral("unknown");
}

#if defined(SHATV_ENABLE_ASR)
QString EnvironmentValue(const char *name) {
    return qEnvironmentVariable(name).trimmed();
}

QString AsrModelSourceName(app::AsrModelInstallSource source) {
    switch (source) {
        case app::AsrModelInstallSource::kAppManaged:
            return QStringLiteral("app_managed");
        case app::AsrModelInstallSource::kDeveloperOverride:
            return QStringLiteral("developer_override");
    }
    return QStringLiteral("unknown");
}

int PositiveEnvironmentInt(const char *name, int default_value, QString *error_message) {
    if (!qEnvironmentVariableIsSet(name)) {
        return default_value;
    }
    bool ok = false;
    const int value = qEnvironmentVariableIntValue(name, &ok);
    if (!ok) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("%1 must be an integer").arg(QString::fromUtf8(name));
        }
        return -1;
    }
    if (value <= 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("%1 must be greater than zero").arg(QString::fromUtf8(name));
        }
        return -1;
    }
    return value;
}

bool EnvironmentBool(const char *name, bool default_value, bool *value, QString *error_message) {
    if (value == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("missing boolean output for %1").arg(QString::fromLatin1(name));
        }
        return false;
    }
    if (!qEnvironmentVariableIsSet(name)) {
        *value = default_value;
        return true;
    }

    const QString text = qEnvironmentVariable(name).trimmed().toLower();
    if (text == QStringLiteral("1") || text == QStringLiteral("true") ||
        text == QStringLiteral("yes") || text == QStringLiteral("on")) {
        *value = true;
        return true;
    }
    if (text == QStringLiteral("0") || text == QStringLiteral("false") ||
        text == QStringLiteral("no") || text == QStringLiteral("off")) {
        *value = false;
        return true;
    }

    if (error_message != nullptr) {
        *error_message = QStringLiteral("%1 must be one of 1/0/true/false/on/off/yes/no")
                             .arg(QString::fromLatin1(name));
    }
    return false;
}
#endif

domain::RetryPolicy BackendRetryPolicy(const domain::MediaSourceDescriptor &source) {
    if (source.source_kind == domain::SourceKind::kRemotePlaylistFetch) {
        return {};
    }
    return source.retry_policy;
}

class VideoOnlyPresentationClock final {
   public:
    bool WaitUntilDue(const media::video::VideoFrame &frame,
                      const std::atomic_bool &abort_requested,
                      QString *error_message) {
        if (frame.pts_usecs < 0) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("decoded video frame has no PTS");
            }
            return false;
        }

        if (!started_) {
            // 视频-only 没有音频主时钟；用首帧 PTS 归零，再用单调时钟保持真实播放速度。
            started_ = true;
            first_pts_usecs_ = frame.pts_usecs;
            elapsed_timer_.start();
            return true;
        }

        const qint64 target_usecs = frame.pts_usecs - first_pts_usecs_;
        while (!abort_requested.load()) {
            const qint64 elapsed_usecs = elapsed_timer_.nsecsElapsed() / 1000;
            const qint64 remaining_usecs = target_usecs - elapsed_usecs;
            if (remaining_usecs <= kVideoEarlyToleranceUsecs) {
                return true;
            }

            const auto sleep_ms = static_cast<unsigned long>(
                std::clamp<qint64>(remaining_usecs / 1000, 1, static_cast<qint64>(kVideoDrainSleepMillis)));
            QThread::msleep(sleep_ms);
        }

        return true;
    }

   private:
    bool started_ = false;
    qint64 first_pts_usecs_ = 0;
    QElapsedTimer elapsed_timer_;
};

}  // namespace

FfmpegPlayerBackend::FfmpegPlayerBackend(QObject *parent) : PlayerBackend(parent), audio_output_(this) {}

FfmpegPlayerBackend::~FfmpegPlayerBackend() {
    StopWorker();
#if defined(SHATV_ENABLE_ASR)
    StopAsrSession();
    JoinAllAsrStartupThreads();
#endif
    audio_output_.Stop();
}

void FfmpegPlayerBackend::Load(const domain::MediaSourceDescriptor &source) {
    qCInfo(app::log_ffmpeg).noquote()
        << "FFmpeg load requested"
        << "name=" << source.name
        << "kind=" << SourceKindName(source.source_kind)
        << "url=" << app::RedactUrlForLog(source.url);
    StopWorker();
    audio_output_.Stop();
    ClearSpeechSubtitle();
#if defined(SHATV_ENABLE_ASR)
    StopAsrSession();
#endif
    video_frame_queue_.Clear();

    current_source_ = source;
    EmitSnapshot(domain::PlaybackState::kLoading, tr("Loading %1 with FFmpeg").arg(source.name));

    if (!video_only_mode_.load()) {
        QString audio_error;
        if (!audio_output_.Start(48000, 2, &audio_error)) {
            qCWarning(app::log_ffmpeg).noquote() << "FFmpeg audio output start failed reason=" << audio_error;
            EmitSnapshot(domain::PlaybackState::kError, tr("FFmpeg audio output failed: %1").arg(audio_error));
            return;
        }
        audio_output_.SetVolume(volume_.load());
        audio_output_.SetMuted(muted_.load());
    }

    abort_requested_ = false;
    worker_thread_ = std::unique_ptr<QThread>(QThread::create([this, source]() { RunPlaybackSession(source); }));
    worker_thread_->start();
}

void FfmpegPlayerBackend::Play() {
    audio_output_.Resume();
    EmitSnapshot(domain::PlaybackState::kPlaying, tr("Playing %1").arg(current_source_.name));
}

void FfmpegPlayerBackend::Pause() {
    audio_output_.Pause();
    EmitSnapshot(domain::PlaybackState::kPaused, tr("Paused %1").arg(current_source_.name));
}

void FfmpegPlayerBackend::Stop() {
    StopWorker();
    audio_output_.Stop();
    ClearSpeechSubtitle();
#if defined(SHATV_ENABLE_ASR)
    StopAsrSession();
#endif
    current_source_ = {};
    EmitSnapshot(domain::PlaybackState::kIdle, tr("Stopped"));
}

void FfmpegPlayerBackend::SetVolume(int volume) {
    volume_ = std::clamp(volume, 0, 100);
    audio_output_.SetVolume(volume_.load());
    EmitControlSnapshot(tr("Volume %1").arg(volume_.load()));
}

void FfmpegPlayerBackend::SetMuted(bool muted) {
    muted_ = muted;
    audio_output_.SetMuted(muted_.load());
    EmitControlSnapshot(muted_.load() ? tr("Muted") : tr("Unmuted"));
}

void FfmpegPlayerBackend::SetSpeechSubtitleEnabled(bool enabled) {
    speech_subtitle_enabled_ = enabled;
    if (!enabled) {
#if defined(SHATV_ENABLE_ASR)
        StopAsrSession();
#endif
        ClearSpeechSubtitle();
        return;
    }
#if defined(SHATV_ENABLE_ASR)
    bool playback_active = false;
    {
        QMutexLocker locker(&snapshot_mutex_);
        playback_active = last_snapshot_.state == domain::PlaybackState::kLoading ||
                          last_snapshot_.state == domain::PlaybackState::kPlaying ||
                          last_snapshot_.state == domain::PlaybackState::kPaused ||
                          last_snapshot_.state == domain::PlaybackState::kBuffering ||
                          last_snapshot_.state == domain::PlaybackState::kRetrying;
    }
    if (playback_active) {
        RequestAsrStartup(current_source_);
    }
#endif
}

void FfmpegPlayerBackend::AttachVideoSink(VideoFrameSink *sink) {
    video_sink_.store(sink);
}

void FfmpegPlayerBackend::DetachVideoSink(VideoFrameSink *sink) {
    VideoFrameSink *expected = sink;
    video_sink_.compare_exchange_strong(expected, nullptr);
}

namespace {

void PresentFrameToSink(std::atomic<VideoFrameSink *> *sink, const media::video::VideoFrame &frame) {
    VideoFrameSink *target = sink->load();
    if (target != nullptr) {
        target->PresentVideoFrame(frame);
    }
}

bool PresentVideoOnlyFrame(std::atomic<VideoFrameSink *> *sink,
                           const media::video::VideoFrame &frame,
                           VideoOnlyPresentationClock *presentation_clock,
                           const std::atomic_bool &abort_requested,
                           QString *error_message) {
    if (!presentation_clock->WaitUntilDue(frame, abort_requested, error_message)) {
        return false;
    }
    if (abort_requested.load()) {
        return true;
    }

    PresentFrameToSink(sink, frame);
    return true;
}

}  // namespace

void FfmpegPlayerBackend::SetVideoOnlyMode(bool video_only) {
    video_only_mode_ = video_only;
}

#if defined(SHATV_ENABLE_ASR)
bool FfmpegPlayerBackend::TapAsrAudioFrame(const AVFrame &frame, QString *error_message) {
    std::shared_ptr<media::asr::StreamingRecognizerWorker> worker_to_stop;
    QMutexLocker locker(&asr_mutex_);
    if (asr_runtime_state_ != AsrRuntimeState::kActive || !asr_worker_) {
        return true;
    }

    media::asr::PcmChunk chunk;
    if (!asr_pcm_converter_.ConvertFrame(frame, &chunk, error_message)) {
        return false;
    }
    if (chunk.samples.empty()) {
        return true;
    }

    QString enqueue_error;
    if (!asr_worker_->Enqueue(std::move(chunk), &enqueue_error)) {
        qCWarning(app::log_ffmpeg).noquote()
            << "ASR audio tap stopped"
            << "reason=" << enqueue_error;
        worker_to_stop = StopAsrSessionLocked(AsrRuntimeState::kFailed);
        locker.unlock();
        if (worker_to_stop) {
            worker_to_stop->Stop();
        }
        ClearSpeechSubtitle();
    }
    return true;
}

void FfmpegPlayerBackend::RequestAsrStartup(const domain::MediaSourceDescriptor &source) {
    if (!speech_subtitle_enabled_.load()) {
        return;
    }
    if (source.url.isEmpty()) {
        return;
    }

    std::shared_ptr<media::asr::StreamingRecognizerWorker> worker_to_stop;
    bool should_stop_worker = false;
    media::asr::StreamingRecognizerConfig config;
    QString config_error;
    quint64 generation = 0;
    {
        QMutexLocker locker(&asr_mutex_);
        PruneFinishedAsrStartupThreadsLocked();
        worker_to_stop = StopAsrSessionLocked(AsrRuntimeState::kEnabledPendingStart);
        generation = asr_generation_;
        if (!speech_subtitle_enabled_.load()) {
            asr_runtime_state_ = AsrRuntimeState::kDisabled;
            should_stop_worker = true;
        } else if (!BuildAsrConfig(source, generation, &config, &config_error)) {
            asr_runtime_state_ = AsrRuntimeState::kFailed;
            should_stop_worker = true;
            qCWarning(app::log_ffmpeg).noquote()
                << "ASR async startup failed before launch"
                << "name=" << source.name
                << "reason=" << config_error;
        }

        if (should_stop_worker) {
            // 旧会话已经从状态中摘除；不要依赖析构隐式释放模型资源。
            locker.unlock();
            if (worker_to_stop) {
                worker_to_stop->Stop();
            }
            return;
        }

        asr_runtime_state_ = AsrRuntimeState::kStarting;
        const auto done = std::make_shared<std::atomic_bool>(false);
        asr_startup_threads_.push_back(AsrStartupThread{
            .thread = std::thread([this, done, generation, source_name = source.name, config]() mutable {
                QElapsedTimer elapsed_timer;
                elapsed_timer.start();
                qCInfo(app::log_ffmpeg).noquote()
                    << "ASR async startup recognizer creation started"
                    << "name=" << source_name
                    << "generation=" << generation
                    << "modelDir=" << config.model_dir
                    << "provider=" << config.provider;

                auto worker = std::make_shared<media::asr::StreamingRecognizerWorker>();
                QString startup_error;
                if (!worker->StartSession(config, &startup_error)) {
                    worker.reset();
                }

                CompleteAsrStartup(generation,
                                    source_name,
                                    config.model_dir,
                                    config.provider,
                                    config.max_queued_chunks,
                                    elapsed_timer.elapsed(),
                                    std::move(worker),
                                    startup_error);
                done->store(true);
            }),
            .done = done,
        });
    }
    if (worker_to_stop) {
        worker_to_stop->Stop();
    }
    qCInfo(app::log_ffmpeg).noquote()
        << "ASR async startup requested"
        << "name=" << source.name
        << "generation=" << generation;
}

bool FfmpegPlayerBackend::BuildAsrConfig(const domain::MediaSourceDescriptor &source,
                                         quint64 generation,
                                         media::asr::StreamingRecognizerConfig *config,
                                         QString *error_message) {
    if (config == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("missing ASR config output");
        }
        return false;
    }
    const app::AsrModelService model_service;
    const app::AsrModelStatus model_status = model_service.InstalledModelStatus();
    if (!model_status.Available()) {
        if (error_message != nullptr) {
            *error_message = model_status.message.isEmpty()
                                 ? QStringLiteral("ASR model is not available")
                                 : model_status.message;
        }
        qCInfo(app::log_ffmpeg).noquote()
            << "ASR async startup skipped"
            << "name=" << source.name
            << "reason=" << (error_message == nullptr ? QStringLiteral("ASR model is not available")
                                                       : *error_message);
        return false;
    }

    config->model_dir = model_status.model_dir;
    const QString encoder_name = EnvironmentValue("SHATV_ASR_ENCODER_NAME");
    const QString decoder_name = EnvironmentValue("SHATV_ASR_DECODER_NAME");
    const QString tokens_name = EnvironmentValue("SHATV_ASR_TOKENS_NAME");
    const QString provider = EnvironmentValue("SHATV_ASR_PROVIDER");
    const app::AsrModelFileSet files = model_service.EffectiveFiles();
    config->encoder_name = files.encoder_name;
    config->decoder_name = files.decoder_name;
    config->tokens_name = files.tokens_name;
    if (!encoder_name.isEmpty()) {
        config->encoder_name = encoder_name;
    }
    if (!decoder_name.isEmpty()) {
        config->decoder_name = decoder_name;
    }
    if (!tokens_name.isEmpty()) {
        config->tokens_name = tokens_name;
    }
    if (!provider.isEmpty()) {
        config->provider = provider;
    }
    config->num_threads = PositiveEnvironmentInt("SHATV_ASR_NUM_THREADS", kDefaultAsrNumThreads, error_message);
    if (config->num_threads <= 0) {
        return false;
    }
    config->max_queued_chunks =
        PositiveEnvironmentInt("SHATV_ASR_MAX_QUEUED_CHUNKS", kDefaultAsrMaxQueuedChunks, error_message);
    if (config->max_queued_chunks <= 0) {
        return false;
    }
    if (!EnvironmentBool("SHATV_ASR_BENCHMARK_LOG", false, &config->benchmark_logging, error_message)) {
        return false;
    }
    config->result_callback = [this, generation, source_name = source.name](
                                  const media::asr::StreamingRecognitionResult &result) {
        HandleAsrRecognitionResult(generation, source_name, result);
    };
    qCInfo(app::log_ffmpeg).noquote()
        << "ASR config resolved"
        << "name=" << source.name
        << "modelSource=" << AsrModelSourceName(model_status.source)
        << "modelDir=" << config->model_dir;
    return true;
}

void FfmpegPlayerBackend::CompleteAsrStartup(quint64 generation,
                                             const QString &source_name,
                                             const QString &model_dir,
                                             const QString &provider,
                                             int max_queued_chunks,
                                             qint64 elapsed_ms,
                                             std::shared_ptr<media::asr::StreamingRecognizerWorker> worker,
                                             const QString &error_message) {
    std::shared_ptr<media::asr::StreamingRecognizerWorker> worker_to_stop;
    {
        QMutexLocker locker(&asr_mutex_);
        if (generation != asr_generation_ ||
            !speech_subtitle_enabled_.load() ||
            asr_runtime_state_ != AsrRuntimeState::kStarting) {
            worker_to_stop = std::move(worker);
            qCInfo(app::log_ffmpeg).noquote()
                << "ASR async startup discarded"
                << "name=" << source_name
                << "generation=" << generation
                << "currentGeneration=" << asr_generation_
                << "elapsedMs=" << elapsed_ms;
        } else if (!worker) {
            asr_runtime_state_ = AsrRuntimeState::kFailed;
            qCWarning(app::log_ffmpeg).noquote()
                << "ASR async startup failed"
                << "name=" << source_name
                << "generation=" << generation
                << "elapsedMs=" << elapsed_ms
                << "reason=" << error_message;
        } else {
            asr_worker_ = std::move(worker);
            asr_pcm_converter_.Reset();
            asr_runtime_state_ = AsrRuntimeState::kActive;
            qCInfo(app::log_ffmpeg).noquote()
                << "ASR async startup completed"
                << "name=" << source_name
                << "generation=" << generation
                << "elapsedMs=" << elapsed_ms
                << "modelDir=" << model_dir
                << "provider=" << provider
                << "maxQueuedChunks=" << max_queued_chunks;
        }
    }
    if (worker_to_stop) {
        worker_to_stop->Stop();
    }
}

void FfmpegPlayerBackend::HandleAsrRecognitionResult(
    quint64 generation,
    const QString &source_name,
    const media::asr::StreamingRecognitionResult &result) {
    {
        QMutexLocker locker(&asr_mutex_);
        if (generation != asr_generation_ ||
            !speech_subtitle_enabled_.load() ||
            asr_runtime_state_ != AsrRuntimeState::kActive) {
            return;
        }
    }

    qCInfo(app::log_ffmpeg).noquote()
        << "ASR recognition result"
        << "name=" << source_name
        << "final=" << result.is_final
        << "latencyMs=" << result.latency_ms
        << "text=" << result.text;
    EmitSpeechSubtitleResult(result.text, result.is_final, result.latency_ms);
}

bool FfmpegPlayerBackend::FinishAsrSession(QString *error_message) {
    std::shared_ptr<media::asr::StreamingRecognizerWorker> worker;
    quint64 generation = 0;
    {
        QMutexLocker locker(&asr_mutex_);
        if (asr_runtime_state_ != AsrRuntimeState::kActive || !asr_worker_) {
            if (asr_runtime_state_ == AsrRuntimeState::kStarting ||
                asr_runtime_state_ == AsrRuntimeState::kEnabledPendingStart) {
                StopAsrSessionLocked(AsrRuntimeState::kDisabled);
            }
            return true;
        }
        worker = asr_worker_;
        generation = asr_generation_;
    }

    const bool ok = worker->FinishSession(error_message);
    {
        QMutexLocker locker(&asr_mutex_);
        if (generation == asr_generation_ && asr_worker_ == worker) {
            asr_worker_.reset();
            asr_pcm_converter_.Reset();
            ++asr_generation_;
            asr_runtime_state_ = AsrRuntimeState::kDisabled;
        }
    }
    return ok;
}

void FfmpegPlayerBackend::StopAsrSession() {
    std::shared_ptr<media::asr::StreamingRecognizerWorker> worker_to_stop;
    {
        QMutexLocker locker(&asr_mutex_);
        worker_to_stop = StopAsrSessionLocked(AsrRuntimeState::kDisabled);
    }
    if (worker_to_stop) {
        worker_to_stop->Stop();
    }
    ClearSpeechSubtitle();
}

std::shared_ptr<media::asr::StreamingRecognizerWorker> FfmpegPlayerBackend::StopAsrSessionLocked(
    AsrRuntimeState next_state) {
    std::shared_ptr<media::asr::StreamingRecognizerWorker> worker_to_stop = std::move(asr_worker_);
    asr_worker_.reset();
    asr_pcm_converter_.Reset();
    ++asr_generation_;
    asr_runtime_state_ = next_state;
    return worker_to_stop;
}

void FfmpegPlayerBackend::PruneFinishedAsrStartupThreadsLocked() {
    auto it = asr_startup_threads_.begin();
    while (it != asr_startup_threads_.end()) {
        if (it->done != nullptr && it->done->load()) {
            if (it->thread.joinable()) {
                it->thread.join();
            }
            it = asr_startup_threads_.erase(it);
        } else {
            ++it;
        }
    }
}

void FfmpegPlayerBackend::JoinAllAsrStartupThreads() {
    std::vector<AsrStartupThread> startup_threads;
    {
        QMutexLocker locker(&asr_mutex_);
        startup_threads.swap(asr_startup_threads_);
    }
    for (AsrStartupThread &startup_thread : startup_threads) {
        if (startup_thread.thread.joinable()) {
            startup_thread.thread.join();
        }
    }
}
#endif

void FfmpegPlayerBackend::RunPlaybackSession(domain::MediaSourceDescriptor source) {
    int attempt = 1;
    while (!abort_requested_) {
        qCInfo(app::log_ffmpeg).noquote()
            << "FFmpeg playback attempt started"
            << "name=" << source.name
            << "kind=" << SourceKindName(source.source_kind)
            << "attempt=" << attempt
            << "url=" << app::RedactUrlForLog(source.url);
        video_frame_queue_.Clear();
        const PlaybackPipelineResult result =
            video_only_mode_.load() ? RunVideoPipeline(source) : RunMediaPipeline(source);
#if defined(SHATV_ENABLE_ASR)
        StopAsrSession();
#endif
        if (result.status == PlaybackPipelineStatus::kAborted || abort_requested_) {
            qCInfo(app::log_ffmpeg).noquote() << "FFmpeg playback aborted name=" << source.name;
            return;
        }
        if (result.status == PlaybackPipelineStatus::kFinished) {
            if (ReconnectOnEndOfStream(source.source_kind)) {
                const RetryDecision retry_decision = RetryDecisionForFailure(BackendRetryPolicy(source), attempt);
                if (!retry_decision.should_retry) {
                    qCWarning(app::log_ffmpeg).noquote()
                        << "FFmpeg live stream ended without retry"
                        << "name=" << source.name
                        << "attempt=" << attempt;
                    EmitSnapshotForSource(source,
                                          domain::PlaybackState::kError,
                                          tr("FFmpeg live stream ended after %1 attempts").arg(attempt));
                    return;
                }

                qCInfo(app::log_ffmpeg).noquote()
                    << "FFmpeg live stream reconnect scheduled"
                    << "name=" << source.name
                    << "nextAttempt=" << retry_decision.next_attempt
                    << "delayMs=" << retry_decision.delay_ms;
                EmitSnapshotForSource(source,
                                      domain::PlaybackState::kRetrying,
                                      tr("Reconnecting %1 with FFmpeg (attempt %2/%3)")
                                          .arg(source.name)
                                          .arg(retry_decision.next_attempt)
                                          .arg(BackendRetryPolicy(source).max_attempts),
                                      retry_decision.next_attempt);
                SleepBeforeRetry(retry_decision.delay_ms);
                attempt = retry_decision.next_attempt;
                continue;
            }
            qCInfo(app::log_ffmpeg).noquote() << "FFmpeg playback finished name=" << source.name;
            EmitSnapshotForSource(source, domain::PlaybackState::kIdle, tr("Finished %1").arg(source.name));
            return;
        }

        const domain::RetryPolicy retry_policy = BackendRetryPolicy(source);
        const RetryDecision retry_decision = RetryDecisionForFailure(retry_policy, attempt);
        if (!retry_decision.should_retry) {
            qCWarning(app::log_ffmpeg).noquote()
                << "FFmpeg playback failed"
                << "name=" << source.name
                << "attempt=" << attempt
                << "reason=" << result.error_message;
            EmitSnapshotForSource(source, domain::PlaybackState::kError, result.error_message);
            return;
        }

        qCWarning(app::log_ffmpeg).noquote()
            << "FFmpeg playback retry scheduled"
            << "name=" << source.name
            << "nextAttempt=" << retry_decision.next_attempt
            << "delayMs=" << retry_decision.delay_ms
            << "reason=" << result.error_message;
        EmitSnapshotForSource(source,
                              domain::PlaybackState::kRetrying,
                              tr("Retrying %1 with FFmpeg (attempt %2/%3)")
                                  .arg(source.name)
                                  .arg(retry_decision.next_attempt)
                                  .arg(retry_policy.max_attempts),
                              retry_decision.next_attempt);
        SleepBeforeRetry(retry_decision.delay_ms);
        attempt = retry_decision.next_attempt;
    }
}

PlaybackPipelineResult FfmpegPlayerBackend::RunMediaPipeline(domain::MediaSourceDescriptor source) {
    media::demux::Demuxer demuxer;
    const media::demux::DemuxerOpenOptions demux_options{
        .abort_requested = &abort_requested_,
    };
    QString error_message;
    if (!demuxer.Open(source, demux_options, &error_message)) {
        return PipelineFailed(tr("FFmpeg demux failed: %1").arg(error_message));
    }

    if (!demuxer.HasVideoStream()) {
        return RunAudioPipeline(std::move(source));
    }

    if (!demuxer.HasAudioStream()) {
        return RunVideoPipeline(std::move(source));
    }

    media::decode::AudioDecoder audio_decoder;
    if (!audio_decoder.Open(demuxer.AudioCodecParameters(), &error_message)) {
        return PipelineFailed(tr("FFmpeg audio decoder failed: %1").arg(error_message));
    }

    media::decode::VideoDecoder video_decoder;
    if (!video_decoder.Open(demuxer.VideoCodecParameters(), demuxer.VideoTimeBaseSeconds(), &error_message)) {
        return PipelineFailed(tr("FFmpeg video decoder failed: %1").arg(error_message));
    }

#if defined(SHATV_ENABLE_ASR)
    RequestAsrStartup(source);
#endif

    AvPacketPtr packet(av_packet_alloc());
    if (packet == nullptr) {
        return PipelineFailed(tr("FFmpeg packet allocation failed"));
    }

    bool emitted_playing = false;
    qint64 first_video_pts_usecs = -1;
    while (!abort_requested_) {
        const media::demux::ReadPacketResult read_result = demuxer.ReadNextMediaPacket(packet.get(), &error_message);
        if (read_result == media::demux::ReadPacketResult::kEndOfFile) {
            break;
        }
        if (read_result == media::demux::ReadPacketResult::kError) {
            return PipelineFailed(tr("FFmpeg demux failed: %1").arg(error_message));
        }

        if (demuxer.IsAudioPacket(*packet)) {
            std::vector<media::decode::AvFramePtr> frames;
            if (!audio_decoder.DecodePacket(*packet, &frames, &error_message)) {
                return PipelineFailed(tr("FFmpeg audio decode failed: %1").arg(error_message));
            }

            for (const auto &frame : frames) {
                if (abort_requested_) {
                    return PipelineAborted();
                }
#if defined(SHATV_ENABLE_ASR)
                if (!TapAsrAudioFrame(*frame, &error_message)) {
                    return PipelineFailed(tr("FFmpeg ASR audio tap failed: %1").arg(error_message));
                }
#endif
                if (!audio_output_.WriteFrame(*frame, &error_message)) {
                    return PipelineFailed(tr("FFmpeg audio output failed: %1").arg(error_message));
                }
                DrainVideoFrames(source, &emitted_playing, false, &first_video_pts_usecs);
            }
            continue;
        }

        if (demuxer.IsVideoPacket(*packet)) {
            std::vector<media::video::VideoFrame> frames;
            if (!video_decoder.DecodePacket(*packet, &frames, &error_message)) {
                return PipelineFailed(tr("FFmpeg video decode failed: %1").arg(error_message));
            }

            for (auto &frame : frames) {
                if (abort_requested_) {
                    return PipelineAborted();
                }
                // A/V 管线不能让解码无限跑在音频时钟前面；但启动预缓冲期间音频时钟尚未推进。
                // 此时不能等待音频时钟，否则 worker 会在 QAudioSink 恢复前自锁。
                while (!abort_requested_ && video_frame_queue_.IsFull()) {
                    if (!audio_output_.PlaybackStarted()) {
                        video_frame_queue_.DropOldest();
                        continue;
                    }
                    DrainVideoFrames(source, &emitted_playing, true, &first_video_pts_usecs);
                }
                video_frame_queue_.Push(std::move(frame));
                DrainVideoFrames(source, &emitted_playing, false, &first_video_pts_usecs);
            }
        }
    }

    if (!abort_requested_) {
        std::vector<media::decode::AvFramePtr> audio_frames;
        if (!audio_decoder.Flush(&audio_frames, &error_message)) {
            return PipelineFailed(tr("FFmpeg audio flush failed: %1").arg(error_message));
        }
        for (const auto &frame : audio_frames) {
#if defined(SHATV_ENABLE_ASR)
            if (!TapAsrAudioFrame(*frame, &error_message)) {
                return PipelineFailed(tr("FFmpeg ASR audio tap failed: %1").arg(error_message));
            }
#endif
            if (!audio_output_.WriteFrame(*frame, &error_message)) {
                return PipelineFailed(tr("FFmpeg audio output failed: %1").arg(error_message));
            }
        }
        audio_output_.FinishStartupPrebuffer();
#if defined(SHATV_ENABLE_ASR)
        if (!FinishAsrSession(&error_message)) {
            return PipelineFailed(tr("FFmpeg ASR worker failed: %1").arg(error_message));
        }
#endif

        std::vector<media::video::VideoFrame> video_frames;
        if (!video_decoder.Flush(&video_frames, &error_message)) {
            return PipelineFailed(tr("FFmpeg video flush failed: %1").arg(error_message));
        }
        for (auto &frame : video_frames) {
            video_frame_queue_.Push(std::move(frame));
        }
        DrainVideoFrames(source, &emitted_playing, true, &first_video_pts_usecs);
    }

    return abort_requested_ ? PipelineAborted() : PipelineFinished();
}

PlaybackPipelineResult FfmpegPlayerBackend::RunAudioPipeline(domain::MediaSourceDescriptor source) {
    media::demux::Demuxer demuxer;
    const media::demux::DemuxerOpenOptions demux_options{
        .abort_requested = &abort_requested_,
    };
    QString error_message;
    if (!demuxer.Open(source, demux_options, &error_message)) {
        return PipelineFailed(tr("FFmpeg demux failed: %1").arg(error_message));
    }
    if (!demuxer.HasAudioStream()) {
        return PipelineFailed(tr("FFmpeg demux failed: no audio stream found"));
    }

    media::decode::AudioDecoder decoder;
    if (!decoder.Open(demuxer.AudioCodecParameters(), &error_message)) {
        return PipelineFailed(tr("FFmpeg audio decoder failed: %1").arg(error_message));
    }

#if defined(SHATV_ENABLE_ASR)
    RequestAsrStartup(source);
#endif

    AvPacketPtr packet(av_packet_alloc());
    if (packet == nullptr) {
        return PipelineFailed(tr("FFmpeg packet allocation failed"));
    }

    bool emitted_playing = false;
    while (!abort_requested_) {
        const media::demux::ReadPacketResult read_result = demuxer.ReadNextAudioPacket(packet.get(), &error_message);
        if (read_result == media::demux::ReadPacketResult::kEndOfFile) {
            break;
        }
        if (read_result == media::demux::ReadPacketResult::kError) {
            return PipelineFailed(tr("FFmpeg demux failed: %1").arg(error_message));
        }

        std::vector<media::decode::AvFramePtr> frames;
        if (!decoder.DecodePacket(*packet, &frames, &error_message)) {
            return PipelineFailed(tr("FFmpeg audio decode failed: %1").arg(error_message));
        }

        for (const auto &frame : frames) {
            if (abort_requested_) {
                return PipelineAborted();
            }
#if defined(SHATV_ENABLE_ASR)
            if (!TapAsrAudioFrame(*frame, &error_message)) {
                return PipelineFailed(tr("FFmpeg ASR audio tap failed: %1").arg(error_message));
            }
#endif
            if (!audio_output_.WriteFrame(*frame, &error_message)) {
                return PipelineFailed(tr("FFmpeg audio output failed: %1").arg(error_message));
            }
            if (!emitted_playing) {
                emitted_playing = true;
                EmitSnapshotForSource(source, domain::PlaybackState::kPlaying, tr("Playing %1").arg(source.name));
            }
        }
    }

    if (!abort_requested_) {
        std::vector<media::decode::AvFramePtr> frames;
        if (!decoder.Flush(&frames, &error_message)) {
            return PipelineFailed(tr("FFmpeg audio flush failed: %1").arg(error_message));
        }
        for (const auto &frame : frames) {
#if defined(SHATV_ENABLE_ASR)
            if (!TapAsrAudioFrame(*frame, &error_message)) {
                return PipelineFailed(tr("FFmpeg ASR audio tap failed: %1").arg(error_message));
            }
#endif
            if (!audio_output_.WriteFrame(*frame, &error_message)) {
                return PipelineFailed(tr("FFmpeg audio output failed: %1").arg(error_message));
            }
            if (!emitted_playing) {
                emitted_playing = true;
                EmitSnapshotForSource(source, domain::PlaybackState::kPlaying, tr("Playing %1").arg(source.name));
            }
        }
        audio_output_.FinishStartupPrebuffer();
#if defined(SHATV_ENABLE_ASR)
        if (!FinishAsrSession(&error_message)) {
            return PipelineFailed(tr("FFmpeg ASR worker failed: %1").arg(error_message));
        }
#endif
    }

    return abort_requested_ ? PipelineAborted() : PipelineFinished();
}

PlaybackPipelineResult FfmpegPlayerBackend::RunVideoPipeline(domain::MediaSourceDescriptor source) {
    media::demux::Demuxer demuxer;
    const media::demux::DemuxerOpenOptions demux_options{
        .abort_requested = &abort_requested_,
    };
    QString error_message;
    if (!demuxer.Open(source, demux_options, &error_message)) {
        return PipelineFailed(tr("FFmpeg demux failed: %1").arg(error_message));
    }
    if (!demuxer.HasVideoStream()) {
        return PipelineFailed(tr("FFmpeg demux failed: no video stream found"));
    }

    media::decode::VideoDecoder decoder;
    if (!decoder.Open(demuxer.VideoCodecParameters(), demuxer.VideoTimeBaseSeconds(), &error_message)) {
        return PipelineFailed(tr("FFmpeg video decoder failed: %1").arg(error_message));
    }

    AvPacketPtr packet(av_packet_alloc());
    if (packet == nullptr) {
        return PipelineFailed(tr("FFmpeg packet allocation failed"));
    }

    VideoOnlyPresentationClock presentation_clock;
    bool emitted_playing = false;
    while (!abort_requested_) {
        const media::demux::ReadPacketResult read_result = demuxer.ReadNextVideoPacket(packet.get(), &error_message);
        if (read_result == media::demux::ReadPacketResult::kEndOfFile) {
            break;
        }
        if (read_result == media::demux::ReadPacketResult::kError) {
            return PipelineFailed(tr("FFmpeg demux failed: %1").arg(error_message));
        }

        std::vector<media::video::VideoFrame> frames;
        if (!decoder.DecodePacket(*packet, &frames, &error_message)) {
            return PipelineFailed(tr("FFmpeg video decode failed: %1").arg(error_message));
        }

        for (auto &frame : frames) {
            if (abort_requested_) {
                return PipelineAborted();
            }
            video_frame_queue_.Push(std::move(frame));
            media::video::VideoFrame queued_frame;
            while (video_frame_queue_.TryPop(&queued_frame)) {
                if (!PresentVideoOnlyFrame(
                        &video_sink_, queued_frame, &presentation_clock, abort_requested_, &error_message)) {
                    return PipelineFailed(tr("FFmpeg video pacing failed: %1").arg(error_message));
                }
                if (abort_requested_) {
                    return PipelineAborted();
                }
                if (!emitted_playing) {
                    emitted_playing = true;
                    EmitSnapshotForSource(source, domain::PlaybackState::kPlaying, tr("Playing %1").arg(source.name));
                }
            }
        }
    }

    if (!abort_requested_) {
        std::vector<media::video::VideoFrame> frames;
        if (!decoder.Flush(&frames, &error_message)) {
            return PipelineFailed(tr("FFmpeg video flush failed: %1").arg(error_message));
        }
        for (const media::video::VideoFrame &frame : frames) {
            if (!PresentVideoOnlyFrame(&video_sink_, frame, &presentation_clock, abort_requested_, &error_message)) {
                return PipelineFailed(tr("FFmpeg video pacing failed: %1").arg(error_message));
            }
            if (abort_requested_) {
                return PipelineAborted();
            }
            if (!emitted_playing) {
                emitted_playing = true;
                EmitSnapshotForSource(source, domain::PlaybackState::kPlaying, tr("Playing %1").arg(source.name));
            }
        }
    }

    return abort_requested_ ? PipelineAborted() : PipelineFinished();
}

void FfmpegPlayerBackend::DrainVideoFrames(const domain::MediaSourceDescriptor &source,
                                           bool *emitted_playing,
                                           bool wait_for_due_frame,
                                           qint64 *first_video_pts_usecs) {
    while (!abort_requested_) {
        media::video::VideoFrame frame;
        if (!video_frame_queue_.TryPeek(&frame)) {
            return;
        }

        if (!audio_output_.PlaybackStarted()) {
            if (!wait_for_due_frame) {
                return;
            }
            QThread::msleep(kVideoDrainSleepMillis);
            continue;
        }

        if (frame.pts_usecs >= 0) {
            if (first_video_pts_usecs != nullptr && *first_video_pts_usecs < 0) {
                *first_video_pts_usecs = frame.pts_usecs;
            }
            const qint64 audio_clock_usecs = audio_output_.ProcessedUsecs();
            const qint64 video_clock_usecs =
                first_video_pts_usecs != nullptr ? std::max<qint64>(0, frame.pts_usecs - *first_video_pts_usecs)
                                                 : frame.pts_usecs;
            const qint64 delta_usecs = video_clock_usecs - audio_clock_usecs;
            if (delta_usecs < -kVideoLateDropUsecs) {
                video_frame_queue_.DropOldest();
                continue;
            }
            if (delta_usecs > kVideoEarlyToleranceUsecs) {
                if (!wait_for_due_frame) {
                    return;
                }
                QThread::msleep(kVideoDrainSleepMillis);
                continue;
            }
        }

        if (!video_frame_queue_.TryPop(&frame)) {
            return;
        }

        PresentFrameToSink(&video_sink_, frame);
        if (emitted_playing != nullptr && !*emitted_playing) {
            *emitted_playing = true;
            EmitSnapshotForSource(source, domain::PlaybackState::kPlaying, tr("Playing %1").arg(source.name));
        }
    }
}

void FfmpegPlayerBackend::SleepBeforeRetry(int delay_ms) {
    int remaining_ms = std::max(0, delay_ms);
    while (!abort_requested_ && remaining_ms > 0) {
        const int sleep_ms = std::min(remaining_ms, 50);
        QThread::msleep(static_cast<unsigned long>(sleep_ms));
        remaining_ms -= sleep_ms;
    }
}

void FfmpegPlayerBackend::StopWorker() {
    abort_requested_ = true;
    if (worker_thread_ != nullptr) {
        worker_thread_->wait();
        worker_thread_.reset();
    }
}

void FfmpegPlayerBackend::EmitSpeechSubtitleResult(QString text, bool is_final, qint64 latency_ms) {
    QMetaObject::invokeMethod(
        this,
        [this, text = std::move(text), is_final, latency_ms]() {
            emit SpeechSubtitleChanged(text, is_final, latency_ms);
        },
        Qt::QueuedConnection);
}

void FfmpegPlayerBackend::ClearSpeechSubtitle() {
    QMetaObject::invokeMethod(this, [this]() { emit SpeechSubtitleCleared(); }, Qt::QueuedConnection);
}

void FfmpegPlayerBackend::EmitSnapshot(domain::PlaybackState state, const QString &message, int retry_count) {
    EmitSnapshotForSource(current_source_, state, message, retry_count);
}

void FfmpegPlayerBackend::EmitSnapshotForSource(const domain::MediaSourceDescriptor &source,
                                                domain::PlaybackState state,
                                                const QString &message,
                                                int retry_count) {
    domain::PlayerSnapshot snapshot;
    snapshot.state = state;
    snapshot.channel_id = source.id;
    snapshot.channel_name = source.name;
    snapshot.message = message;
    snapshot.volume = volume_.load();
    snapshot.muted = muted_.load();
    snapshot.retry_count = retry_count;
    {
        QMutexLocker locker(&snapshot_mutex_);
        last_snapshot_ = snapshot;
    }
    emit SnapshotChanged(snapshot);
}

void FfmpegPlayerBackend::EmitControlSnapshot(const QString &message) {
    domain::PlayerSnapshot snapshot;
    {
        QMutexLocker locker(&snapshot_mutex_);
        snapshot = last_snapshot_;
    }

    snapshot.message = message;
    snapshot.volume = volume_.load();
    snapshot.muted = muted_.load();

    {
        QMutexLocker locker(&snapshot_mutex_);
        last_snapshot_ = snapshot;
    }
    emit SnapshotChanged(snapshot);
}

}  // namespace shatv::player
