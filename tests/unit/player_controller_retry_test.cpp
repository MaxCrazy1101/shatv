#include <QtTest>

#include <QSignalSpy>

#include "application/player_controller.h"
#include "domain/channel.h"
#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/player_backend.h"

namespace {

using shatv::application::PlayerController;
using shatv::domain::Channel;
using shatv::domain::PlaybackState;
using shatv::domain::PlayerSnapshot;
using shatv::player::PlayerBackend;

class ScriptedBackend final : public PlayerBackend {
    Q_OBJECT

   public:
    explicit ScriptedBackend(QObject *parent = nullptr) : PlayerBackend(parent) {}

    void Load(const Channel &channel) override {
        ++load_count_;
        last_channel_ = channel;

        PlayerSnapshot snapshot;
        snapshot.state = PlaybackState::kLoading;
        snapshot.channel_id = channel.id;
        snapshot.channel_name = channel.name;
        snapshot.message = QString("Loading %1").arg(channel.name);
        emit SnapshotChanged(snapshot);
    }

    void Play() override {}
    void Pause() override {}
    void Stop() override {}
    void SetVolume(int volume) override { Q_UNUSED(volume); }
    void SetMuted(bool muted) override { Q_UNUSED(muted); }

    void EmitError(const QString &message) {
        PlayerSnapshot snapshot;
        snapshot.state = PlaybackState::kError;
        snapshot.channel_id = last_channel_.id;
        snapshot.channel_name = last_channel_.name;
        snapshot.message = message;
        emit SnapshotChanged(snapshot);
    }

    int load_count() const { return load_count_; }

   private:
    Channel last_channel_;
    int load_count_ = 0;
};

class PlayerControllerRetryTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        qRegisterMetaType<PlayerSnapshot>("shatv::domain::PlayerSnapshot");
    }

    void error_triggers_single_retry();
    void stop_cancels_pending_retry();
};

void PlayerControllerRetryTest::error_triggers_single_retry() {
    ScriptedBackend backend;
    PlayerController controller(&backend);
    QSignalSpy snapshot_spy(&controller, &PlayerController::PlaybackSnapshotChanged);

    const Channel channel{
        .id = "demo-news",
        .name = "Demo News",
        .url = QUrl("https://example.com/demo-news.m3u8"),
        .group = "News",
    };

    controller.PlayChannel(channel);
    backend.EmitError("Network timeout");

    QTRY_COMPARE_WITH_TIMEOUT(backend.load_count(), 2, 1000);
    QVERIFY(snapshot_spy.count() >= 2);

    bool saw_retrying_snapshot = false;
    for (const QList<QVariant> &signal_arguments : snapshot_spy) {
        const PlayerSnapshot snapshot = signal_arguments.at(0).value<PlayerSnapshot>();
        if (snapshot.state == PlaybackState::kRetrying && snapshot.retry_count == 1) {
            saw_retrying_snapshot = true;
            break;
        }
    }

    QVERIFY(saw_retrying_snapshot);
}

void PlayerControllerRetryTest::stop_cancels_pending_retry() {
    ScriptedBackend backend;
    PlayerController controller(&backend);

    const Channel channel{
        .id = "demo-news",
        .name = "Demo News",
        .url = QUrl("https://example.com/demo-news.m3u8"),
        .group = "News",
    };

    controller.PlayChannel(channel);
    backend.EmitError("Network timeout");
    controller.Stop();

    QTest::qWait(500);
    QCOMPARE(backend.load_count(), 1);
}

}  // namespace

QTEST_GUILESS_MAIN(PlayerControllerRetryTest)

#include "player_controller_retry_test.moc"
