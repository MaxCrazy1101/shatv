#include "media/audio/audio_output.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>
#include <QIODevice>
#include <QMediaDevices>
#include <QMutex>
#include <QMutexLocker>

#include "media/ffmpeg_error.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace shatv::media::audio {

namespace {

constexpr AVSampleFormat kOutputSampleFormat = AV_SAMPLE_FMT_S16;
constexpr QAudioFormat::SampleFormat kQtOutputSampleFormat = QAudioFormat::Int16;

AVChannelLayout DefaultLayout(int channel_count) {
    AVChannelLayout layout{};
    av_channel_layout_default(&layout, channel_count);
    return layout;
}

}  // namespace

class AudioOutput::PcmBufferDevice final : public QIODevice {
   public:
    explicit PcmBufferDevice(QObject *parent = nullptr) : QIODevice(parent) {}

    void Append(const QByteArray &pcm) {
        QMutexLocker locker(&mutex_);
        buffer_.append(pcm);
    }

    void Clear() {
        QMutexLocker locker(&mutex_);
        buffer_.clear();
    }

    qint64 readData(char *data, qint64 max_size) override {
        if (max_size <= 0) {
            return 0;
        }

        QMutexLocker locker(&mutex_);
        if (buffer_.isEmpty()) {
            std::memset(data, 0, static_cast<std::size_t>(max_size));
            return max_size;
        }

        const qint64 bytes_to_copy = std::min<qint64>(max_size, buffer_.size());
        std::memcpy(data, buffer_.constData(), static_cast<std::size_t>(bytes_to_copy));
        buffer_.remove(0, static_cast<qsizetype>(bytes_to_copy));
        if (bytes_to_copy < max_size) {
            std::memset(data + bytes_to_copy, 0, static_cast<std::size_t>(max_size - bytes_to_copy));
            return max_size;
        }
        return bytes_to_copy;
    }

    qint64 writeData(const char *, qint64) override {
        return -1;
    }

    qint64 bytesAvailable() const override {
        QMutexLocker locker(&mutex_);
        return buffer_.size() + QIODevice::bytesAvailable();
    }

   private:
    mutable QMutex mutex_;
    QByteArray buffer_;
};

AudioOutput::AudioOutput(QObject *parent) : QObject(parent) {
    clock_timer_.setInterval(10);
    QObject::connect(&clock_timer_, &QTimer::timeout, this, [this]() {
        clock_.SetPositionUsecs(sink_ != nullptr ? sink_->processedUSecs() : 0);
    });
}

AudioOutput::~AudioOutput() {
    Stop();
    ResetResampler();
}

bool AudioOutput::Start(int sample_rate, int channel_count, QString *error_message) {
    Stop();
    clock_.Reset();

    output_sample_rate_ = sample_rate > 0 ? sample_rate : 48000;
    output_channel_count_ = channel_count > 0 ? channel_count : 2;

    QAudioFormat format;
    format.setSampleRate(output_sample_rate_);
    format.setChannelCount(output_channel_count_);
    format.setSampleFormat(kQtOutputSampleFormat);
    if (output_channel_count_ == 1) {
        format.setChannelConfig(QAudioFormat::ChannelConfigMono);
    } else if (output_channel_count_ == 2) {
        format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
    }

    const QAudioDevice output_device = QMediaDevices::defaultAudioOutput();
    if (output_device.isNull()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("no default audio output device");
        }
        return false;
    }

    if (!output_device.isFormatSupported(format)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("default audio output does not support %1 Hz %2-channel Int16 PCM")
                                 .arg(output_sample_rate_)
                                 .arg(output_channel_count_);
        }
        return false;
    }

    output_format_ = format;
    pcm_device_ = std::make_unique<PcmBufferDevice>();
    pcm_device_->open(QIODevice::ReadOnly);
    sink_ = std::make_unique<QAudioSink>(output_device, format);
    sink_->setBufferSize(format.bytesForDuration(500 * 1000));
    ApplyVolume();
    sink_->start(pcm_device_.get());
    if (sink_->isNull() || sink_->error() != QtAudio::NoError) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("QAudioSink failed to start");
        }
        Stop();
        return false;
    }

    clock_timer_.start();
    return true;
}

bool AudioOutput::WriteFrame(const AVFrame &frame, QString *error_message) {
    if (pcm_device_ == nullptr || !output_format_.isValid()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("audio output is not started");
        }
        return false;
    }

    if (!EnsureResampler(frame, error_message)) {
        return false;
    }

    const int64_t delay = swr_get_delay(resampler_, frame.sample_rate);
    const int output_samples = static_cast<int>(
        av_rescale_rnd(delay + frame.nb_samples, output_sample_rate_, frame.sample_rate, AV_ROUND_UP));
    if (output_samples <= 0) {
        return true;
    }

    QByteArray pcm(output_samples * output_format_.bytesPerFrame(), Qt::Uninitialized);
    auto *output_data = reinterpret_cast<uint8_t *>(pcm.data());
    uint8_t *output_planes[] = {output_data};
    const auto *input_planes = const_cast<const uint8_t **>(frame.extended_data);

    const int converted_samples =
        swr_convert(resampler_, output_planes, output_samples, input_planes, frame.nb_samples);
    if (converted_samples < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("swr_convert failed: %1").arg(FfmpegErrorString(converted_samples));
        }
        return false;
    }

    pcm.resize(converted_samples * output_format_.bytesPerFrame());
    pcm_device_->Append(pcm);
    return true;
}

void AudioOutput::Stop() {
    clock_timer_.stop();
    clock_.Reset();
    if (sink_ != nullptr) {
        sink_->stop();
        sink_.reset();
    }
    if (pcm_device_ != nullptr) {
        pcm_device_->close();
        pcm_device_->Clear();
        pcm_device_.reset();
    }
    output_format_ = QAudioFormat{};
    ResetResampler();
}

void AudioOutput::Pause() {
    if (sink_ != nullptr) {
        sink_->suspend();
    }
}

void AudioOutput::Resume() {
    if (sink_ != nullptr) {
        sink_->resume();
    }
}

void AudioOutput::SetVolume(int volume) {
    volume_ = std::clamp(volume, 0, 100);
    ApplyVolume();
}

void AudioOutput::SetMuted(bool muted) {
    muted_ = muted;
    ApplyVolume();
}

qint64 AudioOutput::ProcessedUsecs() const {
    return clock_.PositionUsecs();
}

void AudioOutput::ResetResampler() {
    if (resampler_ != nullptr) {
        swr_free(&resampler_);
    }
}

bool AudioOutput::EnsureResampler(const AVFrame &frame, QString *error_message) {
    if (resampler_ != nullptr) {
        return true;
    }

    if (frame.sample_rate <= 0 || frame.nb_samples <= 0 || frame.extended_data == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("invalid decoded audio frame");
        }
        return false;
    }

    AVChannelLayout input_layout{};
    bool input_layout_needs_uninit = false;
    if (av_channel_layout_check(&frame.ch_layout) != 0) {
        if (av_channel_layout_copy(&input_layout, &frame.ch_layout) < 0) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("failed to copy input channel layout");
            }
            return false;
        }
        input_layout_needs_uninit = true;
    } else {
        input_layout = DefaultLayout(std::max(1, frame.ch_layout.nb_channels));
        input_layout_needs_uninit = true;
    }

    AVChannelLayout output_layout = DefaultLayout(output_channel_count_);
    const int alloc_result = swr_alloc_set_opts2(&resampler_,
                                                &output_layout,
                                                kOutputSampleFormat,
                                                output_sample_rate_,
                                                &input_layout,
                                                static_cast<AVSampleFormat>(frame.format),
                                                frame.sample_rate,
                                                0,
                                                nullptr);
    av_channel_layout_uninit(&output_layout);
    if (input_layout_needs_uninit) {
        av_channel_layout_uninit(&input_layout);
    }

    if (alloc_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("swr_alloc_set_opts2 failed: %1").arg(FfmpegErrorString(alloc_result));
        }
        return false;
    }

    const int init_result = swr_init(resampler_);
    if (init_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("swr_init failed: %1").arg(FfmpegErrorString(init_result));
        }
        ResetResampler();
        return false;
    }

    return true;
}

void AudioOutput::ApplyVolume() {
    if (sink_ != nullptr) {
        sink_->setVolume(muted_ ? 0.0 : static_cast<qreal>(volume_) / 100.0);
    }
}

}  // namespace shatv::media::audio
