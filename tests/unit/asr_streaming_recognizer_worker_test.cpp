#include <QtTest>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QFile>

#include "media/asr/streaming_recognizer_worker.h"

namespace {

using shatv::media::asr::PcmChunk;
using shatv::media::asr::StreamingRecognitionResult;
using shatv::media::asr::StreamingRecognizerConfig;
using shatv::media::asr::StreamingRecognizerWorker;

struct WaveFixture {
    std::vector<float> samples;
    int sample_rate = 0;
};

uint16_t ReadU16Le(const QByteArray &data, qsizetype offset) {
    return static_cast<uint16_t>(static_cast<unsigned char>(data.at(offset))) |
           static_cast<uint16_t>(static_cast<unsigned char>(data.at(offset + 1)) << 8);
}

uint32_t ReadU32Le(const QByteArray &data, qsizetype offset) {
    return static_cast<uint32_t>(static_cast<unsigned char>(data.at(offset))) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data.at(offset + 1))) << 8) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data.at(offset + 2))) << 16) |
           (static_cast<uint32_t>(static_cast<unsigned char>(data.at(offset + 3))) << 24);
}

bool ReadPcm16MonoWave(const QString &path, WaveFixture *wave, QString *error_message) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error_message = QStringLiteral("failed to open WAV fixture: %1").arg(path);
        return false;
    }

    const QByteArray data = file.readAll();
    if (data.size() < 44 ||
        std::memcmp(data.constData(), "RIFF", 4) != 0 ||
        std::memcmp(data.constData() + 8, "WAVE", 4) != 0 ||
        std::memcmp(data.constData() + 12, "fmt ", 4) != 0) {
        *error_message = QStringLiteral("fixture is not a canonical RIFF/WAVE file: %1").arg(path);
        return false;
    }

    const uint16_t audio_format = ReadU16Le(data, 20);
    const uint16_t channel_count = ReadU16Le(data, 22);
    const uint32_t sample_rate = ReadU32Le(data, 24);
    const uint16_t bits_per_sample = ReadU16Le(data, 34);
    if (audio_format != 1 || channel_count != 1 || sample_rate == 0 || bits_per_sample != 16) {
        *error_message = QStringLiteral("fixture must be mono PCM signed 16-bit WAV: %1").arg(path);
        return false;
    }

    qsizetype data_chunk_offset = 36;
    while (data_chunk_offset + 8 <= data.size() &&
           std::memcmp(data.constData() + data_chunk_offset, "data", 4) != 0) {
        data_chunk_offset += 8 + static_cast<qsizetype>(ReadU32Le(data, data_chunk_offset + 4));
    }
    if (data_chunk_offset + 8 > data.size()) {
        *error_message = QStringLiteral("fixture is missing data chunk: %1").arg(path);
        return false;
    }

    const qsizetype pcm_offset = data_chunk_offset + 8;
    const qsizetype pcm_size = static_cast<qsizetype>(ReadU32Le(data, data_chunk_offset + 4));
    if (pcm_size <= 0 || pcm_offset + pcm_size > data.size() || pcm_size % 2 != 0) {
        *error_message = QStringLiteral("fixture has invalid data chunk: %1").arg(path);
        return false;
    }

    wave->sample_rate = static_cast<int>(sample_rate);
    wave->samples.clear();
    wave->samples.reserve(static_cast<std::size_t>(pcm_size / 2));
    for (qsizetype offset = pcm_offset; offset < pcm_offset + pcm_size; offset += 2) {
        const auto lo = static_cast<unsigned char>(data.at(offset));
        const auto hi = static_cast<unsigned char>(data.at(offset + 1));
        const int16_t sample = static_cast<int16_t>(static_cast<uint16_t>(lo) |
                                                   static_cast<uint16_t>(hi << 8));
        wave->samples.push_back(static_cast<float>(sample) / 32768.0F);
    }
    return true;
}

class AsrStreamingRecognizerWorkerTest : public QObject {
    Q_OBJECT

   private slots:
    void rejects_missing_model_files();
    void rejects_unsupported_provider();
    void recognizes_fixture_when_configured();
};

void AsrStreamingRecognizerWorkerTest::rejects_missing_model_files() {
    StreamingRecognizerWorker worker;
    StreamingRecognizerConfig config;
    config.model_dir = QStringLiteral("/tmp/shatv-missing-asr-model");
    QString error_message;

    QVERIFY(!worker.StartSession(config, &error_message));
    QVERIFY(error_message.contains(QStringLiteral("required ASR model file is missing")));
}

void AsrStreamingRecognizerWorkerTest::rejects_unsupported_provider() {
    StreamingRecognizerWorker worker;
    StreamingRecognizerConfig config;
    config.model_dir = QStringLiteral("/tmp/shatv-missing-asr-model");
    config.provider = QStringLiteral("bogus");
    QString error_message;

    QVERIFY(!worker.StartSession(config, &error_message));
    QCOMPARE(error_message, QStringLiteral("unsupported ASR provider: bogus"));
}

void AsrStreamingRecognizerWorkerTest::recognizes_fixture_when_configured() {
#if defined(SHATV_ASR_WORKER_MODEL_DIR) && defined(SHATV_ASR_WORKER_AUDIO_FILE)
    WaveFixture wave;
    QString error_message;
    QVERIFY2(ReadPcm16MonoWave(QString::fromUtf8(SHATV_ASR_WORKER_AUDIO_FILE), &wave, &error_message),
             qPrintable(error_message));

    QString last_text;
    QString final_text;
    std::mutex result_mutex;
    StreamingRecognizerConfig config;
    config.model_dir = QString::fromUtf8(SHATV_ASR_WORKER_MODEL_DIR);
    config.max_queued_chunks = 128;
    config.result_callback = [&last_text, &final_text, &result_mutex](const StreamingRecognitionResult &result) {
        std::lock_guard<std::mutex> lock(result_mutex);
        last_text = result.text;
        if (result.is_final) {
            final_text = result.text;
        }
    };

    StreamingRecognizerWorker worker;
    QVERIFY2(worker.StartSession(config, &error_message), qPrintable(error_message));

    constexpr std::size_t kChunkSamples = 3200;
    for (std::size_t offset = 0; offset < wave.samples.size(); offset += kChunkSamples) {
        const std::size_t end = std::min(offset + kChunkSamples, wave.samples.size());
        PcmChunk chunk;
        chunk.sample_rate = wave.sample_rate;
        chunk.channel_count = 1;
        chunk.samples.assign(wave.samples.begin() + static_cast<std::ptrdiff_t>(offset),
                             wave.samples.begin() + static_cast<std::ptrdiff_t>(end));
        QVERIFY2(worker.Enqueue(std::move(chunk), &error_message), qPrintable(error_message));
    }

    QVERIFY2(worker.FinishSession(&error_message), qPrintable(error_message));
    {
        std::lock_guard<std::mutex> lock(result_mutex);
        QVERIFY(!last_text.isEmpty());
        QVERIFY(!final_text.isEmpty());
        QVERIFY(final_text.contains(QStringLiteral("昨天")));
    }
#else
    QSKIP("SHATV_ASR_WORKER_MODEL_DIR and SHATV_ASR_WORKER_AUDIO_FILE are not configured");
#endif
}

}  // namespace

QTEST_GUILESS_MAIN(AsrStreamingRecognizerWorkerTest)
#include "asr_streaming_recognizer_worker_test.moc"
