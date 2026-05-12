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
    void speech_subtitle_control_state_tracks_toggle_request();
    void speech_model_state_tracks_status_and_requests();
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

void AppShellBridgeTest::speech_subtitle_control_state_tracks_toggle_request() {
    ChannelListModel channel_model;
    ChannelFilterModel filter_model;
    filter_model.setSourceModel(&channel_model);
    AppShellBridge bridge(&filter_model);

    QVERIFY(!bridge.SpeechSubtitleEnabled());
    QVERIFY(!bridge.SpeechSubtitleAvailable());
    QCOMPARE(bridge.SpeechSubtitleUnavailableReason(), QString());

    QSignalSpy enabled_spy(&bridge, &AppShellBridge::SpeechSubtitleEnabledChanged);
    QSignalSpy available_spy(&bridge, &AppShellBridge::SpeechSubtitleAvailableChanged);
    QSignalSpy reason_spy(&bridge, &AppShellBridge::SpeechSubtitleUnavailableReasonChanged);
    QSignalSpy request_spy(&bridge, &AppShellBridge::SpeechSubtitleEnabledRequested);

    bridge.SetSpeechSubtitleControlState(false, false, QStringLiteral("missing model"));

    QVERIFY(!bridge.SpeechSubtitleEnabled());
    QVERIFY(!bridge.SpeechSubtitleAvailable());
    QCOMPARE(bridge.SpeechSubtitleUnavailableReason(), QStringLiteral("missing model"));
    QCOMPARE(enabled_spy.size(), 0);
    QCOMPARE(available_spy.size(), 0);
    QCOMPARE(reason_spy.size(), 1);

    bridge.toggleSpeechSubtitleEnabled();
    QCOMPARE(request_spy.size(), 1);
    QCOMPARE(request_spy.takeFirst().at(0).toBool(), true);

    bridge.SetSpeechSubtitleControlState(true, true, QString());

    QVERIFY(bridge.SpeechSubtitleEnabled());
    QVERIFY(bridge.SpeechSubtitleAvailable());
    QCOMPARE(bridge.SpeechSubtitleUnavailableReason(), QString());
    QCOMPARE(enabled_spy.size(), 1);
    QCOMPARE(available_spy.size(), 1);
    QCOMPARE(reason_spy.size(), 2);

    bridge.toggleSpeechSubtitleEnabled();
    QCOMPARE(request_spy.size(), 1);
    QCOMPARE(request_spy.takeFirst().at(0).toBool(), false);
}

void AppShellBridgeTest::speech_model_state_tracks_status_and_requests() {
    ChannelListModel channel_model;
    ChannelFilterModel filter_model;
    filter_model.setSourceModel(&channel_model);
    AppShellBridge bridge(&filter_model);

    QCOMPARE(bridge.SpeechModelStatusToken(), QString());
    QCOMPARE(bridge.SpeechModelStatusText(), QString());
    QVERIFY(!bridge.SpeechModelInstalled());
    QVERIFY(!bridge.SpeechModelDeveloperOverride());
    QVERIFY(!bridge.SpeechModelInstallSupported());
    QVERIFY(!bridge.SpeechModelBusy());

    QSignalSpy status_spy(&bridge, &AppShellBridge::SpeechModelStatusChanged);
    QSignalSpy busy_spy(&bridge, &AppShellBridge::SpeechModelBusyChanged);
    QSignalSpy refresh_spy(&bridge, &AppShellBridge::SpeechModelStatusRefreshRequested);
    QSignalSpy install_spy(&bridge, &AppShellBridge::SpeechModelArchiveInstallRequested);
    QSignalSpy delete_spy(&bridge, &AppShellBridge::SpeechModelDeleteRequested);

    bridge.SetSpeechModelStatus(QStringLiteral("installed"),
                                QStringLiteral("Installed"),
                                QStringLiteral("/tmp/model"),
                                QStringLiteral("Paraformer"),
                                QStringLiteral("v1"),
                                QStringLiteral("https://example.invalid/model"),
                                QStringLiteral("42 MiB"),
                                QStringLiteral("100 MiB"),
                                QStringLiteral("abc123"),
                                QStringLiteral("review-required"),
                                QStringLiteral("sherpa-onnx"),
                                QStringLiteral("/tmp/model"),
                                true,
                                false,
                                true);

    QCOMPARE(status_spy.size(), 1);
    QCOMPARE(bridge.SpeechModelStatusToken(), QStringLiteral("installed"));
    QCOMPARE(bridge.SpeechModelStatusText(), QStringLiteral("Installed"));
    QCOMPARE(bridge.SpeechModelStatusDetail(), QStringLiteral("/tmp/model"));
    QCOMPARE(bridge.SpeechModelName(), QStringLiteral("Paraformer"));
    QCOMPARE(bridge.SpeechModelVersion(), QStringLiteral("v1"));
    QCOMPARE(bridge.SpeechModelSourceUrl(), QStringLiteral("https://example.invalid/model"));
    QCOMPARE(bridge.SpeechModelArchiveSizeText(), QStringLiteral("42 MiB"));
    QCOMPARE(bridge.SpeechModelInstalledSizeText(), QStringLiteral("100 MiB"));
    QCOMPARE(bridge.SpeechModelChecksum(), QStringLiteral("abc123"));
    QCOMPARE(bridge.SpeechModelLicense(), QStringLiteral("review-required"));
    QCOMPARE(bridge.SpeechModelAttribution(), QStringLiteral("sherpa-onnx"));
    QCOMPARE(bridge.SpeechModelDirectory(), QStringLiteral("/tmp/model"));
    QVERIFY(bridge.SpeechModelInstalled());
    QVERIFY(!bridge.SpeechModelDeveloperOverride());
    QVERIFY(bridge.SpeechModelInstallSupported());

    bridge.SetSpeechModelBusy(true);
    QVERIFY(bridge.SpeechModelBusy());
    QCOMPARE(busy_spy.size(), 1);

    bridge.refreshSpeechModelStatus();
    QCOMPARE(refresh_spy.size(), 1);

    bridge.installSpeechModelArchive(QUrl::fromLocalFile(QStringLiteral("/tmp/model.tar.bz2")));
    QCOMPARE(install_spy.size(), 1);
    QCOMPARE(install_spy.takeFirst().at(0).toString(), QStringLiteral("/tmp/model.tar.bz2"));

    bridge.deleteSpeechModel();
    QCOMPARE(delete_spy.size(), 1);
}

}  // namespace

QTEST_GUILESS_MAIN(AppShellBridgeTest)
#include "app_shell_bridge_test.moc"
