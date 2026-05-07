#include "player/ffmpeg_player_backend.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QThread>

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
    QString error_message;
    if (!demuxer.Open(source, &error_message)) {
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
            if (!audio_output_.WriteFrame(*frame, &error_message)) {
                return PipelineFailed(tr("FFmpeg audio output failed: %1").arg(error_message));
            }
        }
        audio_output_.FinishStartupPrebuffer();

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
    QString error_message;
    if (!demuxer.Open(source, &error_message)) {
        return PipelineFailed(tr("FFmpeg demux failed: %1").arg(error_message));
    }
    if (!demuxer.HasAudioStream()) {
        return PipelineFailed(tr("FFmpeg demux failed: no audio stream found"));
    }

    media::decode::AudioDecoder decoder;
    if (!decoder.Open(demuxer.AudioCodecParameters(), &error_message)) {
        return PipelineFailed(tr("FFmpeg audio decoder failed: %1").arg(error_message));
    }

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
            if (!audio_output_.WriteFrame(*frame, &error_message)) {
                return PipelineFailed(tr("FFmpeg audio output failed: %1").arg(error_message));
            }
            if (!emitted_playing) {
                emitted_playing = true;
                EmitSnapshotForSource(source, domain::PlaybackState::kPlaying, tr("Playing %1").arg(source.name));
            }
        }
        audio_output_.FinishStartupPrebuffer();
    }

    return abort_requested_ ? PipelineAborted() : PipelineFinished();
}

PlaybackPipelineResult FfmpegPlayerBackend::RunVideoPipeline(domain::MediaSourceDescriptor source) {
    media::demux::Demuxer demuxer;
    QString error_message;
    if (!demuxer.Open(source, &error_message)) {
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
