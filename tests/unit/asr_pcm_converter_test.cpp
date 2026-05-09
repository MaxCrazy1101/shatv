#include <QtTest>

#include <cmath>
#include <cstdint>
#include <memory>

#include "media/asr/pcm_converter.h"
#include "media/decode/audio_decoder.h"

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
}

namespace {

using shatv::media::asr::PcmChunk;
using shatv::media::asr::PcmConverter;
using shatv::media::decode::AvFrameDeleter;
using shatv::media::decode::AvFramePtr;

AvFramePtr MakeStereoS16Frame(int sample_rate, int sample_count, int16_t left, int16_t right) {
    AvFramePtr frame(av_frame_alloc());
    if (!frame) {
        return nullptr;
    }

    frame->sample_rate = sample_rate;
    frame->nb_samples = sample_count;
    frame->format = AV_SAMPLE_FMT_S16;
    av_channel_layout_default(&frame->ch_layout, 2);
    if (av_frame_get_buffer(frame.get(), 0) < 0) {
        return nullptr;
    }

    auto *samples = reinterpret_cast<int16_t *>(frame->data[0]);
    for (int sample_index = 0; sample_index < sample_count; ++sample_index) {
        samples[sample_index * 2] = left;
        samples[sample_index * 2 + 1] = right;
    }
    return frame;
}

class AsrPcmConverterTest : public QObject {
    Q_OBJECT

   private slots:
    void converts_stereo_48khz_s16_to_mono_16khz_float();
    void rejects_invalid_frames();
};

void AsrPcmConverterTest::converts_stereo_48khz_s16_to_mono_16khz_float() {
    PcmConverter converter;
    PcmChunk chunk;
    QString error_message;
    const AvFramePtr frame = MakeStereoS16Frame(48000, 4800, 8192, 8192);
    QVERIFY(frame != nullptr);

    QVERIFY2(converter.ConvertFrame(*frame, &chunk, &error_message), qPrintable(error_message));

    QCOMPARE(chunk.sample_rate, 16000);
    QCOMPARE(chunk.channel_count, 1);
    QVERIFY(chunk.samples.size() >= 1500);
    QVERIFY(chunk.samples.size() <= 1700);
    QVERIFY(!chunk.samples.empty());

    const float middle_sample = chunk.samples.at(chunk.samples.size() / 2);
    QVERIFY(std::isfinite(middle_sample));
    QVERIFY(std::abs(middle_sample) > 0.01F);
    QVERIFY(std::abs(middle_sample) <= 1.0F);
}

void AsrPcmConverterTest::rejects_invalid_frames() {
    PcmConverter converter;
    PcmChunk chunk;
    QString error_message;
    AvFramePtr frame(av_frame_alloc());
    QVERIFY(frame != nullptr);

    QVERIFY(!converter.ConvertFrame(*frame, &chunk, &error_message));
    QVERIFY(error_message.contains(QStringLiteral("invalid decoded audio frame")));
}

}  // namespace

QTEST_GUILESS_MAIN(AsrPcmConverterTest)
#include "asr_pcm_converter_test.moc"
