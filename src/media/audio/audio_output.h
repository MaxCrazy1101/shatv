#pragma once

#include <atomic>
#include <memory>

#include <QAudioFormat>
#include <QObject>
#include <QString>
#include <QTimer>

#include "media/clock/playback_clock.h"

struct AVFrame;
struct SwrContext;

class QAudioSink;
class QIODevice;

namespace shatv::media::audio {

// Resamples decoded AVFrame audio to PCM and feeds QAudioSink through a
// thread-safe QIODevice.
class AudioOutput final : public QObject {
    Q_OBJECT

   public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput() override;

    AudioOutput(const AudioOutput &) = delete;
    AudioOutput &operator=(const AudioOutput &) = delete;

    bool Start(int sample_rate, int channel_count, QString *error_message);
    bool WriteFrame(const AVFrame &frame, QString *error_message);
    void FinishStartupPrebuffer();
    void Stop();
    void Pause();
    void Resume();
    void SetVolume(int volume);
    void SetMuted(bool muted);
    bool PlaybackStarted() const;
    qint64 ProcessedUsecs() const;

   private:
    class PcmBufferDevice;

    void ResetResampler();
    bool EnsureResampler(const AVFrame &frame, QString *error_message);
    void MaybeResumeAfterPrebuffer(bool force = false);
    void ApplyVolume();

    int volume_ = 50;
    bool muted_ = false;
    int output_sample_rate_ = 48000;
    int output_channel_count_ = 2;
    qint64 start_prebuffer_bytes_ = 0;
    QAudioFormat output_format_;
    QTimer clock_timer_;
    clock::PlaybackClock clock_;
    SwrContext *resampler_ = nullptr;
    std::unique_ptr<PcmBufferDevice> pcm_device_;
    std::unique_ptr<QAudioSink> sink_;
    std::atomic_bool playback_started_ = false;
    std::atomic_bool resume_queued_ = false;
};

}  // namespace shatv::media::audio
