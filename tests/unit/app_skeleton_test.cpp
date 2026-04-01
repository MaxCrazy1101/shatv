#include <QtTest>

#include <QSignalSpy>

#include "application/player_controller.h"
#include "domain/channel.h"
#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/fake_player_backend.h"
#include "ui/models/channel_list_model.h"

namespace {

using shatv::application::PlayerController;
using shatv::domain::Channel;
using shatv::domain::PlaybackState;
using shatv::domain::PlayerSnapshot;
using shatv::player::FakePlayerBackend;
using shatv::ui::models::ChannelListModel;

class AppSkeletonTest : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase() {
        qRegisterMetaType<PlayerSnapshot>("shatv::domain::PlayerSnapshot");
    }

    void player_controller_reaches_playing_state();
    void channel_list_model_marks_current_channel();
};

void AppSkeletonTest::player_controller_reaches_playing_state() {
    FakePlayerBackend backend;
    PlayerController controller(&backend);

    QSignalSpy snapshot_spy(&controller, &PlayerController::PlaybackSnapshotChanged);
    QSignalSpy current_channel_spy(&controller, &PlayerController::CurrentChannelChanged);

    const Channel channel{
        .id = "demo-news",
        .name = "Demo News",
        .url = QUrl("https://example.com/demo-news.m3u8"),
        .group = "News",
    };

    controller.PlayChannel(channel);

    QTRY_VERIFY(snapshot_spy.count() >= 2);
    const PlayerSnapshot last_snapshot = snapshot_spy.back().at(0).value<PlayerSnapshot>();

    QCOMPARE(last_snapshot.state, PlaybackState::kPlaying);
    QCOMPARE(last_snapshot.channel_id, QString("demo-news"));
    QCOMPARE(last_snapshot.channel_name, QString("Demo News"));
    QVERIFY(current_channel_spy.count() >= 1);
}

void AppSkeletonTest::channel_list_model_marks_current_channel() {
    ChannelListModel model;

    model.SetChannels({
        {.id = "demo-news", .name = "Demo News", .url = QUrl("https://example.com/news.m3u8"), .group = "News"},
        {.id = "demo-sports",
         .name = "Demo Sports",
         .url = QUrl("https://example.com/sports.m3u8"),
         .group = "Sports"},
    });

    QCOMPARE(model.rowCount(), 2);

    model.SetCurrentChannelId("demo-news");

    const QModelIndex first = model.index(0, 0);
    const QModelIndex second = model.index(1, 0);

    QCOMPARE(model.data(first, ChannelListModel::kCurrentRole).toBool(), true);
    QCOMPARE(model.data(second, ChannelListModel::kCurrentRole).toBool(), false);
    QCOMPARE(model.ChannelAt(second).id, QString("demo-sports"));
}

}  // namespace

QTEST_GUILESS_MAIN(AppSkeletonTest)

#include "app_skeleton_test.moc"
