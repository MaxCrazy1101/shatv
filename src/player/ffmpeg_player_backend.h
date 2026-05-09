#pragma once

#include <atomic>
#include <memory>

#include <QMutex>
#include <QString>

#include "domain/media_source.h"
#include "domain/player_snapshot.h"
#include "media/asr/pcm_converter.h"
#include "media/audio/audio_output.h"
#include "media/video/video_frame_queue.h"
#include "player/player_backend.h"
#include "player/video_frame_sink.h"

class QThread;
struct AVFrame;

namespace shatv::player {

enum class PlaybackPipelineStatus {
    kFinished,
    kFailed,
    kAborted,
};

struct PlaybackPipelineResult {
    PlaybackPipelineStatus status = PlaybackPipelineStatus::kFinished;
    QString error_message;
};

// Milestone 2 FFmpeg-based backend. Owns playback state mapping and retry
// execution around the media-core demux/decode/output stages.
class FfmpegPlayerBackend final : public PlayerBackend {
    Q_OBJECT

   public:
    explicit FfmpegPlayerBackend(QObject *parent = nullptr);
    ~FfmpegPlayerBackend() override;

    void Load(const domain::MediaSourceDescriptor &source) override;
    void Play() override;
    void Pause() override;
    void Stop() override;
    void SetVolume(int volume) override;
    void SetMuted(bool muted) override;
    void AttachVideoSink(VideoFrameSink *sink);
    void DetachVideoSink(VideoFrameSink *sink);
    void SetVideoOnlyMode(bool video_only);

   private:
    void RunPlaybackSession(domain::MediaSourceDescriptor source);
    PlaybackPipelineResult RunMediaPipeline(domain::MediaSourceDescriptor source);
    PlaybackPipelineResult RunAudioPipeline(domain::MediaSourceDescriptor source);
    PlaybackPipelineResult RunVideoPipeline(domain::MediaSourceDescriptor source);
#if defined(SHATV_ENABLE_ASR)
    bool TapAsrAudioFrame(const AVFrame &frame, QString *error_message);
#endif
    void DrainVideoFrames(const domain::MediaSourceDescriptor &source,
                          bool *emitted_playing,
                          bool wait_for_due_frame,
                          qint64 *first_video_pts_usecs);
    void SleepBeforeRetry(int delay_ms);
    void StopWorker();
    void EmitSnapshot(domain::PlaybackState state, const QString &message, int retry_count = 0);
    void EmitSnapshotForSource(const domain::MediaSourceDescriptor &source,
                               domain::PlaybackState state,
                               const QString &message,
                               int retry_count = 0);
    void EmitControlSnapshot(const QString &message);

    domain::MediaSourceDescriptor current_source_;
    // 控制类操作复用最近一次播放快照，避免音量/静音把播放状态重置。
    domain::PlayerSnapshot last_snapshot_;
    mutable QMutex snapshot_mutex_;
    media::audio::AudioOutput audio_output_;
#if defined(SHATV_ENABLE_ASR)
    media::asr::PcmConverter asr_pcm_converter_;
#endif
    media::video::VideoFrameQueue video_frame_queue_;
    std::unique_ptr<QThread> worker_thread_;
    std::atomic_bool abort_requested_ = false;
    std::atomic_int volume_ = 50;
    std::atomic_bool muted_ = false;
    std::atomic_bool video_only_mode_ = false;
    std::atomic<VideoFrameSink *> video_sink_ = nullptr;
};

}  // namespace shatv::player
