#include <QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "domain/media_source.h"
#include "domain/player_snapshot.h"
#include "player/ffmpeg_player_backend.h"

extern "C" {
#include <libavutil/log.h>
}

namespace {

using shatv::domain::MediaSourceDescriptor;
using shatv::domain::PlaybackState;
using shatv::domain::PlayerSnapshot;
using shatv::domain::RetryPolicy;
using shatv::domain::RetryPolicyForSourceKind;
using shatv::domain::SourceKind;
using shatv::domain::SourceOrigin;
using shatv::player::FfmpegPlayerBackend;

MediaSourceDescriptor MissingMediaSource(SourceKind source_kind) {
    QTemporaryDir temp_dir;
    const QString missing_path = temp_dir.filePath("missing.mp4");

    return MediaSourceDescriptor{
        .id = "missing",
        .name = "Missing Media",
        .url = QUrl::fromLocalFile(missing_path),
        .source_kind = source_kind,
        .origin = SourceOrigin::kManualOpenFile,
        .user_agent = {},
        .retry_policy = RetryPolicyForSourceKind(source_kind),
    };
}

MediaSourceDescriptor LocalMediaSource(const QString &path, SourceKind source_kind, RetryPolicy retry_policy) {
    return MediaSourceDescriptor{
        .id = "local-media",
        .name = QFileInfo(path).fileName(),
        .url = QUrl::fromLocalFile(path),
        .source_kind = source_kind,
        .origin = SourceOrigin::kManualOpenFile,
        .user_agent = {},
        .retry_policy = retry_policy,
    };
}

QString FfmpegSmokeVideoFixturePath() {
    return QDir(QStringLiteral(SHATV_TESTDATA_DIR)).filePath(QStringLiteral("fixtures/ffmpeg_smoke_video.mp4"));
}

QList<PlayerSnapshot> SnapshotsFromSpy(const QSignalSpy &spy) {
    QList<PlayerSnapshot> snapshots;
    for (const QList<QVariant> &arguments : spy) {
        snapshots.push_back(arguments.at(0).value<PlayerSnapshot>());
    }
    return snapshots;
}

bool HasState(const QSignalSpy &spy, PlaybackState state) {
    for (const PlayerSnapshot &snapshot : SnapshotsFromSpy(spy)) {
        if (snapshot.state == state) {
            return true;
        }
    }
    return false;
}

QList<int> RetryCountsFromSpy(const QSignalSpy &spy) {
    QList<int> retry_counts;
    for (const PlayerSnapshot &snapshot : SnapshotsFromSpy(spy)) {
        if (snapshot.state == PlaybackState::kRetrying) {
            retry_counts.push_back(snapshot.retry_count);
        }
    }
    return retry_counts;
}

class FfmpegPlayerBackendRetryTest final : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void corrupt_local_media_surfaces_error_without_retrying();
    void local_file_policy_fails_without_retrying();
    void direct_remote_policy_retries_before_final_error();
    void remote_playlist_fetch_policy_is_not_retried_by_backend();
    void playlist_live_policy_retries_until_max_attempts();
    void playlist_live_eof_reconnects_with_controllable_fixture();
    void video_only_playback_paces_frames_by_pts();
};

void FfmpegPlayerBackendRetryTest::initTestCase() {
    av_log_set_level(AV_LOG_QUIET);
    qRegisterMetaType<PlayerSnapshot>("shatv::domain::PlayerSnapshot");
}

void FfmpegPlayerBackendRetryTest::corrupt_local_media_surfaces_error_without_retrying() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QString corrupt_path = temp_dir.filePath("corrupt.mp4");
    QFile corrupt_file(corrupt_path);
    QVERIFY(corrupt_file.open(QIODevice::WriteOnly));
    corrupt_file.write("not a media container");
    corrupt_file.close();

    FfmpegPlayerBackend backend;
    backend.SetVideoOnlyMode(true);
    QSignalSpy spy(&backend, &FfmpegPlayerBackend::SnapshotChanged);

    backend.Load(LocalMediaSource(corrupt_path, SourceKind::kLocalFile, RetryPolicyForSourceKind(SourceKind::kLocalFile)));

    QTRY_VERIFY_WITH_TIMEOUT(HasState(spy, PlaybackState::kError), 3000);
    QVERIFY(!HasState(spy, PlaybackState::kPlaying));
    QVERIFY(!HasState(spy, PlaybackState::kRetrying));
}

void FfmpegPlayerBackendRetryTest::local_file_policy_fails_without_retrying() {
    FfmpegPlayerBackend backend;
    backend.SetVideoOnlyMode(true);
    QSignalSpy spy(&backend, &FfmpegPlayerBackend::SnapshotChanged);

    backend.Load(MissingMediaSource(SourceKind::kLocalFile));

    QTRY_VERIFY_WITH_TIMEOUT(HasState(spy, PlaybackState::kError), 3000);
    QVERIFY(!HasState(spy, PlaybackState::kRetrying));
}

void FfmpegPlayerBackendRetryTest::direct_remote_policy_retries_before_final_error() {
    FfmpegPlayerBackend backend;
    backend.SetVideoOnlyMode(true);
    QSignalSpy spy(&backend, &FfmpegPlayerBackend::SnapshotChanged);

    backend.Load(MissingMediaSource(SourceKind::kDirectRemoteMedia));

    QTRY_VERIFY_WITH_TIMEOUT(HasState(spy, PlaybackState::kError), 5000);
    bool saw_retrying = false;
    bool saw_error_after_retrying = false;
    for (const PlayerSnapshot &snapshot : SnapshotsFromSpy(spy)) {
        if (snapshot.state == PlaybackState::kRetrying) {
            saw_retrying = true;
            QCOMPARE(snapshot.retry_count, 2);
        }
        if (saw_retrying && snapshot.state == PlaybackState::kError) {
            saw_error_after_retrying = true;
        }
    }

    QVERIFY(saw_retrying);
    QVERIFY(saw_error_after_retrying);
}

void FfmpegPlayerBackendRetryTest::remote_playlist_fetch_policy_is_not_retried_by_backend() {
    FfmpegPlayerBackend backend;
    backend.SetVideoOnlyMode(true);
    QSignalSpy spy(&backend, &FfmpegPlayerBackend::SnapshotChanged);

    backend.Load(MissingMediaSource(SourceKind::kRemotePlaylistFetch));

    QTRY_VERIFY_WITH_TIMEOUT(HasState(spy, PlaybackState::kError), 3000);
    QVERIFY(!HasState(spy, PlaybackState::kRetrying));
}

void FfmpegPlayerBackendRetryTest::playlist_live_policy_retries_until_max_attempts() {
    FfmpegPlayerBackend backend;
    backend.SetVideoOnlyMode(true);
    QSignalSpy spy(&backend, &FfmpegPlayerBackend::SnapshotChanged);

    backend.Load(MissingMediaSource(SourceKind::kPlaylistChannelLive));

    QTRY_VERIFY_WITH_TIMEOUT(HasState(spy, PlaybackState::kError), 7000);
    const QList<int> expected_retry_counts{2, 3, 4, 5};
    QCOMPARE(RetryCountsFromSpy(spy), expected_retry_counts);
}

void FfmpegPlayerBackendRetryTest::playlist_live_eof_reconnects_with_controllable_fixture() {
    const QString fixture_path = FfmpegSmokeVideoFixturePath();
    QVERIFY2(QFile::exists(fixture_path), qPrintable(fixture_path));

    FfmpegPlayerBackend backend;
    backend.SetVideoOnlyMode(true);
    QSignalSpy spy(&backend, &FfmpegPlayerBackend::SnapshotChanged);

    backend.Load(LocalMediaSource(fixture_path,
                                  SourceKind::kPlaylistChannelLive,
                                  RetryPolicy{
                                      .max_attempts = 2,
                                      .initial_delay_ms = 1,
                                      .max_delay_ms = 1,
                                      .backoff = shatv::domain::RetryBackoff::kFixed,
                                  }));

    QTRY_VERIFY_WITH_TIMEOUT(HasState(spy, PlaybackState::kError), 5000);
    QCOMPARE(RetryCountsFromSpy(spy), QList<int>{2});
    QVERIFY(HasState(spy, PlaybackState::kPlaying));
}

void FfmpegPlayerBackendRetryTest::video_only_playback_paces_frames_by_pts() {
    const QString fixture_path = FfmpegSmokeVideoFixturePath();
    QVERIFY2(QFile::exists(fixture_path), qPrintable(fixture_path));

    FfmpegPlayerBackend backend;
    backend.SetVideoOnlyMode(true);
    QSignalSpy spy(&backend, &FfmpegPlayerBackend::SnapshotChanged);

    QElapsedTimer elapsed_timer;
    elapsed_timer.start();
    backend.Load(LocalMediaSource(fixture_path,
                                  SourceKind::kLocalFile,
                                  RetryPolicyForSourceKind(SourceKind::kLocalFile)));

    QTRY_VERIFY_WITH_TIMEOUT(HasState(spy, PlaybackState::kIdle), 4000);
    QVERIFY(HasState(spy, PlaybackState::kPlaying));
    QVERIFY2(elapsed_timer.elapsed() >= 800, qPrintable(QStringLiteral("elapsed=%1ms").arg(elapsed_timer.elapsed())));
}

}  // namespace

QTEST_GUILESS_MAIN(FfmpegPlayerBackendRetryTest)

#include "ffmpeg_player_backend_retry_test.moc"
