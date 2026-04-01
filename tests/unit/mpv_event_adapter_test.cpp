#include <QtTest>

#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/mpv_event_adapter.h"

namespace {

using shatv::domain::PlaybackState;
using shatv::domain::PlayerSnapshot;
using shatv::player::MpvEventAdapter;

class MpvEventAdapterTest : public QObject {
    Q_OBJECT

   private slots:
    void file_loaded_becomes_playing();
    void end_file_eof_becomes_idle();
    void pause_false_without_channel_keeps_previous_state();
    void pause_true_becomes_paused();
    void end_file_error_becomes_error();
};

void MpvEventAdapterTest::file_loaded_becomes_playing() {
    MpvEventAdapter adapter;
    PlayerSnapshot snapshot;

    adapter.ApplyFileLoaded(snapshot, "Demo News");

    QCOMPARE(snapshot.state, PlaybackState::kPlaying);
    QCOMPARE(snapshot.channel_name, QString("Demo News"));
}

void MpvEventAdapterTest::end_file_eof_becomes_idle() {
    MpvEventAdapter adapter;
    PlayerSnapshot snapshot;
    snapshot.channel_id = "demo-news";
    snapshot.channel_name = "Demo News";
    snapshot.state = PlaybackState::kPlaying;

    adapter.ApplyEndFileEof(snapshot);

    QCOMPARE(snapshot.state, PlaybackState::kIdle);
    QCOMPARE(snapshot.message, QString("Finished Demo News"));
}

void MpvEventAdapterTest::pause_false_without_channel_keeps_previous_state() {
    MpvEventAdapter adapter;
    PlayerSnapshot snapshot;
    snapshot.state = PlaybackState::kIdle;
    snapshot.message = "Ready";

    adapter.ApplyPauseChanged(snapshot, false);

    QCOMPARE(snapshot.state, PlaybackState::kIdle);
    QCOMPARE(snapshot.message, QString("Ready"));
    QVERIFY(snapshot.channel_name.isEmpty());
}

void MpvEventAdapterTest::pause_true_becomes_paused() {
    MpvEventAdapter adapter;
    PlayerSnapshot snapshot;
    snapshot.channel_id = "demo-news";
    snapshot.channel_name = "Demo News";
    snapshot.state = PlaybackState::kPlaying;

    adapter.ApplyPauseChanged(snapshot, true);

    QCOMPARE(snapshot.state, PlaybackState::kPaused);
    QCOMPARE(snapshot.message, QString("Paused Demo News"));
}

void MpvEventAdapterTest::end_file_error_becomes_error() {
    MpvEventAdapter adapter;
    PlayerSnapshot snapshot;

    adapter.ApplyEndFileError(snapshot, "Network timeout");

    QCOMPARE(snapshot.state, PlaybackState::kError);
    QCOMPARE(snapshot.message, QString("Network timeout"));
}

}  // namespace

QTEST_GUILESS_MAIN(MpvEventAdapterTest)

#include "mpv_event_adapter_test.moc"
