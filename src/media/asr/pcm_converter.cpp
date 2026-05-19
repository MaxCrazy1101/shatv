#include "media/asr/pcm_converter.h"

#include <algorithm>
#include <cstdint>

#include "media/ffmpeg_error.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace shatv::media::asr {

namespace {

constexpr int kAsrSampleRate = 16000;
constexpr int kAsrChannelCount = 1;
constexpr AVSampleFormat kAsrSampleFormat = AV_SAMPLE_FMT_FLT;

AVChannelLayout DefaultLayout(int channel_count) {
    AVChannelLayout layout{};
    av_channel_layout_default(&layout, channel_count);
    return layout;
}

bool BuildInputLayout(const AVFrame &frame, int input_channel_count, AVChannelLayout *input_layout,
                      QString *error_message) {
    if (av_channel_layout_check(&frame.ch_layout) != 0) {
        const int copy_result = av_channel_layout_copy(input_layout, &frame.ch_layout);
        if (copy_result < 0) {
            if (error_message != nullptr) {
                *error_message =
                    QStringLiteral("failed to copy ASR input channel layout: %1").arg(FfmpegErrorString(copy_result));
            }
            return false;
        }
        return true;
    }

    *input_layout = DefaultLayout(input_channel_count);
    return true;
}

}  // namespace

PcmConverter::~PcmConverter() {
    Reset();
}

bool PcmConverter::ConvertFrame(const AVFrame &frame, PcmChunk *chunk, QString *error_message) {
    if (chunk == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("missing ASR PCM output chunk");
        }
        return false;
    }
    chunk->samples.clear();
    chunk->sample_rate = kAsrSampleRate;
    chunk->channel_count = kAsrChannelCount;

    const int input_channel_count = frame.ch_layout.nb_channels;
    if (frame.sample_rate <= 0 || frame.nb_samples <= 0 || frame.extended_data == nullptr || input_channel_count <= 0 ||
        av_get_sample_fmt_name(static_cast<AVSampleFormat>(frame.format)) == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("invalid decoded audio frame for ASR PCM conversion");
        }
        return false;
    }

    if (!EnsureResampler(frame, input_channel_count, error_message)) {
        return false;
    }

    const int64_t delay = swr_get_delay(resampler_, frame.sample_rate);
    const int output_samples =
        static_cast<int>(av_rescale_rnd(delay + frame.nb_samples, kAsrSampleRate, frame.sample_rate, AV_ROUND_UP));
    if (output_samples <= 0) {
        return true;
    }

    chunk->samples.assign(static_cast<std::size_t>(output_samples), 0.0F);
    auto *output_data = reinterpret_cast<uint8_t *>(chunk->samples.data());
    uint8_t *output_planes[] = {output_data};
    const auto *input_planes = const_cast<const uint8_t **>(frame.extended_data);

    const int converted_samples =
        swr_convert(resampler_, output_planes, output_samples, input_planes, frame.nb_samples);
    if (converted_samples < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("ASR PCM swr_convert failed: %1").arg(FfmpegErrorString(converted_samples));
        }
        return false;
    }

    chunk->samples.resize(static_cast<std::size_t>(converted_samples));
    return true;
}

void PcmConverter::Reset() {
    if (resampler_ != nullptr) {
        swr_free(&resampler_);
    }
    input_sample_rate_ = 0;
    input_format_ = -1;
    input_channel_count_ = 0;
}

bool PcmConverter::EnsureResampler(const AVFrame &frame, int input_channel_count, QString *error_message) {
    if (resampler_ != nullptr && input_sample_rate_ == frame.sample_rate && input_format_ == frame.format &&
        input_channel_count_ == input_channel_count) {
        return true;
    }

    Reset();

    AVChannelLayout input_layout{};
    if (!BuildInputLayout(frame, input_channel_count, &input_layout, error_message)) {
        return false;
    }

    AVChannelLayout output_layout = DefaultLayout(kAsrChannelCount);
    const int alloc_result =
        swr_alloc_set_opts2(&resampler_, &output_layout, kAsrSampleFormat, kAsrSampleRate, &input_layout,
                            static_cast<AVSampleFormat>(frame.format), frame.sample_rate, 0, nullptr);
    av_channel_layout_uninit(&output_layout);
    av_channel_layout_uninit(&input_layout);

    if (alloc_result < 0) {
        if (error_message != nullptr) {
            *error_message =
                QStringLiteral("ASR PCM swr_alloc_set_opts2 failed: %1").arg(FfmpegErrorString(alloc_result));
        }
        return false;
    }

    const int init_result = swr_init(resampler_);
    if (init_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("ASR PCM swr_init failed: %1").arg(FfmpegErrorString(init_result));
        }
        Reset();
        return false;
    }

    input_sample_rate_ = frame.sample_rate;
    input_format_ = frame.format;
    input_channel_count_ = input_channel_count;
    return true;
}

}  // namespace shatv::media::asr
