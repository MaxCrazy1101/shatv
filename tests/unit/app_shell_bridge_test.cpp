#include <QtTest>

#include "ui/models/channel_filter_model.h"
#include "ui/models/channel_list_model.h"
#include "ui/shell/app_shell_bridge.h"

namespace {

using shatv::ui::models::ChannelFilterModel;
using shatv::ui::models::ChannelListModel;
using shatv::ui::shell::AppShellBridge;

class AppShellBridgeTest : public QObject {
    Q_OBJECT

   private slots:
    void speech_subtitle_state_tracks_result_and_clear();
};

void AppShellBridgeTest::speech_subtitle_state_tracks_result_and_clear() {
    ChannelListModel channel_model;
    ChannelFilterModel filter_model;
    filter_model.setSourceModel(&channel_model);
    AppShellBridge bridge(&filter_model);

    QCOMPARE(bridge.SpeechSubtitleText(), QString());
    QVERIFY(!bridge.SpeechSubtitleActive());
    QVERIFY(!bridge.SpeechSubtitleFinal());
    QCOMPARE(bridge.SpeechSubtitleLatencyMs(), -1);
    QCOMPARE(bridge.SpeechSubtitleStatusText(), QString());

    QSignalSpy text_spy(&bridge, &AppShellBridge::SpeechSubtitleTextChanged);
    QSignalSpy active_spy(&bridge, &AppShellBridge::SpeechSubtitleActiveChanged);
    QSignalSpy final_spy(&bridge, &AppShellBridge::SpeechSubtitleFinalChanged);
    QSignalSpy latency_spy(&bridge, &AppShellBridge::SpeechSubtitleLatencyMsChanged);
    QSignalSpy status_spy(&bridge, &AppShellBridge::SpeechSubtitleStatusTextChanged);

    bridge.SetSpeechSubtitle(QStringLiteral("实时字幕"), false, 123);

    QCOMPARE(bridge.SpeechSubtitleText(), QStringLiteral("实时字幕"));
    QVERIFY(bridge.SpeechSubtitleActive());
    QVERIFY(!bridge.SpeechSubtitleFinal());
    QCOMPARE(bridge.SpeechSubtitleLatencyMs(), 123);
    QCOMPARE(bridge.SpeechSubtitleStatusText(), QStringLiteral("Delay 123 ms"));
    QCOMPARE(text_spy.size(), 1);
    QCOMPARE(active_spy.size(), 1);
    QCOMPARE(final_spy.size(), 0);
    QCOMPARE(latency_spy.size(), 1);
    QCOMPARE(status_spy.size(), 1);

    bridge.SetSpeechSubtitle(QStringLiteral("实时字幕完成"), true, 456);

    QCOMPARE(bridge.SpeechSubtitleText(), QStringLiteral("实时字幕完成"));
    QVERIFY(bridge.SpeechSubtitleActive());
    QVERIFY(bridge.SpeechSubtitleFinal());
    QCOMPARE(bridge.SpeechSubtitleLatencyMs(), 456);
    QCOMPARE(bridge.SpeechSubtitleStatusText(), QStringLiteral("Delay 456 ms"));
    QCOMPARE(text_spy.size(), 2);
    QCOMPARE(active_spy.size(), 1);
    QCOMPARE(final_spy.size(), 1);
    QCOMPARE(latency_spy.size(), 2);
    QCOMPARE(status_spy.size(), 2);

    bridge.ClearSpeechSubtitle();

    QCOMPARE(bridge.SpeechSubtitleText(), QString());
    QVERIFY(!bridge.SpeechSubtitleActive());
    QVERIFY(!bridge.SpeechSubtitleFinal());
    QCOMPARE(bridge.SpeechSubtitleLatencyMs(), -1);
    QCOMPARE(bridge.SpeechSubtitleStatusText(), QString());
    QCOMPARE(text_spy.size(), 3);
    QCOMPARE(active_spy.size(), 2);
    QCOMPARE(final_spy.size(), 2);
    QCOMPARE(latency_spy.size(), 3);
    QCOMPARE(status_spy.size(), 3);
}

}  // namespace

QTEST_GUILESS_MAIN(AppShellBridgeTest)
#include "app_shell_bridge_test.moc"
