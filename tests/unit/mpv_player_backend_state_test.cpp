#include <QtTest>

#include <QSignalSpy>

#include "domain/channel.h"
#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/mpv_player_backend.h"

namespace {

using shatv::domain::Channel;
using shatv::domain::PlaybackState;
using shatv::domain::PlayerSnapshot;
using shatv::player::MpvPlayerBackend;

class MpvPlayerBackendStateTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        qRegisterMetaType<PlayerSnapshot>("shatv::domain::PlayerSnapshot");
    }

    void volume_update_preserves_paused_state();
    void mute_update_preserves_paused_state();
};

void MpvPlayerBackendStateTest::volume_update_preserves_paused_state() {
    MpvPlayerBackend backend;
    QSignalSpy snapshot_spy(&backend, &MpvPlayerBackend::SnapshotChanged);

    const Channel channel{
        .id = "demo-news",
        .name = "Demo News",
        .url = QUrl("https://example.com/demo-news.m3u8"),
        .group = "News",
    };

    backend.Load(channel);
    backend.Pause();
    snapshot_spy.clear();

    backend.SetVolume(25);

    QVERIFY(!snapshot_spy.isEmpty());
    const PlayerSnapshot snapshot = snapshot_spy.back().at(0).value<PlayerSnapshot>();
    QCOMPARE(snapshot.state, PlaybackState::kPaused);
    QCOMPARE(snapshot.message, QString("Volume 25"));
}

void MpvPlayerBackendStateTest::mute_update_preserves_paused_state() {
    MpvPlayerBackend backend;
    QSignalSpy snapshot_spy(&backend, &MpvPlayerBackend::SnapshotChanged);

    const Channel channel{
        .id = "demo-news",
        .name = "Demo News",
        .url = QUrl("https://example.com/demo-news.m3u8"),
        .group = "News",
    };

    backend.Load(channel);
    backend.Pause();
    snapshot_spy.clear();

    backend.SetMuted(true);

    QVERIFY(!snapshot_spy.isEmpty());
    const PlayerSnapshot snapshot = snapshot_spy.back().at(0).value<PlayerSnapshot>();
    QCOMPARE(snapshot.state, PlaybackState::kPaused);
    QCOMPARE(snapshot.message, QString("Muted"));
}

}  // namespace

QTEST_GUILESS_MAIN(MpvPlayerBackendStateTest)

#include "mpv_player_backend_state_test.moc"
