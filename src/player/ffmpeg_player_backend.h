#pragma once

#include <atomic>
#include <memory>

#include <QString>

#include "domain/media_source.h"
#include "media/audio/audio_output.h"
#include "media/video/video_frame_queue.h"
#include "player/player_backend.h"
#include "player/video_frame_sink.h"

class QThread;

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

    domain::MediaSourceDescriptor current_source_;
    media::audio::AudioOutput audio_output_;
    media::video::VideoFrameQueue video_frame_queue_;
    std::unique_ptr<QThread> worker_thread_;
    std::atomic_bool abort_requested_ = false;
    std::atomic_int volume_ = 50;
    std::atomic_bool muted_ = false;
    std::atomic_bool video_only_mode_ = false;
    std::atomic<VideoFrameSink *> video_sink_ = nullptr;
};

}  // namespace shatv::player
