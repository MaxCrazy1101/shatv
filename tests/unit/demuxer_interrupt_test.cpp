#include <QtTest>

#include <atomic>

#include <QByteArray>
#include <QElapsedTimer>

#include "domain/media_source.h"
#include "media/demux/demuxer.h"

namespace {

using shatv::domain::MediaSourceDescriptor;
using shatv::domain::RetryPolicyForSourceKind;
using shatv::domain::SourceKind;
using shatv::domain::SourceOrigin;
using shatv::media::demux::Demuxer;
using shatv::media::demux::DemuxerOpenOptions;

class ScopedEnvironmentVariable final {
   public:
    explicit ScopedEnvironmentVariable(const char *name) : name_(name), had_value_(qEnvironmentVariableIsSet(name)) {
        if (had_value_) {
            old_value_ = qgetenv(name);
        }
    }

    ~ScopedEnvironmentVariable() {
        if (had_value_) {
            qputenv(name_, old_value_);
        } else {
            qunsetenv(name_);
        }
    }

    ScopedEnvironmentVariable(const ScopedEnvironmentVariable &) = delete;
    ScopedEnvironmentVariable &operator=(const ScopedEnvironmentVariable &) = delete;

   private:
    const char *name_ = nullptr;
    bool had_value_ = false;
    QByteArray old_value_;
};

MediaSourceDescriptor RemoteMediaSource() {
    return MediaSourceDescriptor{
        .id = QStringLiteral("remote"),
        .name = QStringLiteral("Remote Media"),
        .url = QUrl(QStringLiteral("http://127.0.0.1:1/stream.m3u8")),
        .source_kind = SourceKind::kPlaylistChannelLive,
        .origin = SourceOrigin::kRemotePlaylist,
        .user_agent = {},
        .retry_policy = RetryPolicyForSourceKind(SourceKind::kPlaylistChannelLive),
    };
}

class DemuxerInterruptTest final : public QObject {
    Q_OBJECT

   private slots:
    void abort_requested_before_open_short_circuits();
    void invalid_remote_open_timeout_fails_before_network_io();
};

void DemuxerInterruptTest::abort_requested_before_open_short_circuits() {
    std::atomic_bool abort_requested = true;
    Demuxer demuxer;
    QString error_message;

    QElapsedTimer elapsed_timer;
    elapsed_timer.start();
    const bool opened =
        demuxer.Open(RemoteMediaSource(), DemuxerOpenOptions{.abort_requested = &abort_requested}, &error_message);

    QVERIFY(!opened);
    QVERIFY2(elapsed_timer.elapsed() < 100, qPrintable(QStringLiteral("elapsed=%1ms").arg(elapsed_timer.elapsed())));
    QCOMPARE(error_message, QStringLiteral("FFmpeg demux open aborted before start"));
}

void DemuxerInterruptTest::invalid_remote_open_timeout_fails_before_network_io() {
    ScopedEnvironmentVariable open_timeout("SHATV_FFMPEG_OPEN_TIMEOUT_MS");
    qputenv("SHATV_FFMPEG_OPEN_TIMEOUT_MS", "not-an-integer");

    Demuxer demuxer;
    QString error_message;
    const bool opened = demuxer.Open(RemoteMediaSource(), &error_message);

    QVERIFY(!opened);
    QCOMPARE(error_message, QStringLiteral("SHATV_FFMPEG_OPEN_TIMEOUT_MS must be an integer"));
}

}  // namespace

QTEST_GUILESS_MAIN(DemuxerInterruptTest)

#include "demuxer_interrupt_test.moc"
