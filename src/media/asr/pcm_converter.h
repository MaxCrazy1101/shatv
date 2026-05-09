#pragma once

#include <vector>

#include <QString>

struct AVFrame;
struct SwrContext;

namespace shatv::media::asr {

struct PcmChunk {
    std::vector<float> samples;
    int sample_rate = 16000;
    int channel_count = 1;
};

// Converts decoded playback audio into the mono 16 kHz float PCM format used
// by the ASR pipeline. This is intentionally independent from AudioOutput so
// playback volume, mute state, and output-device format cannot affect ASR input.
class PcmConverter final {
   public:
    PcmConverter() = default;
    ~PcmConverter();

    PcmConverter(const PcmConverter &) = delete;
    PcmConverter &operator=(const PcmConverter &) = delete;

    bool ConvertFrame(const AVFrame &frame, PcmChunk *chunk, QString *error_message);
    void Reset();

   private:
    bool EnsureResampler(const AVFrame &frame, int input_channel_count, QString *error_message);

    SwrContext *resampler_ = nullptr;
    int input_sample_rate_ = 0;
    int input_format_ = -1;
    int input_channel_count_ = 0;
};

}  // namespace shatv::media::asr
