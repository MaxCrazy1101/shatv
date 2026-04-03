#include <QtTest>

#include <QCoreApplication>
#include <QTranslator>

#include "domain/playback_state.h"
#include "ui/panels/playback_status_panel.h"
#include "ui/panels/player_control_bar.h"
#include "ui/windows/main_window.h"

namespace {

using shatv::domain::PlaybackState;
using shatv::domain::PlaybackStateName;
using shatv::ui::panels::PlaybackStatusPanel;
using shatv::ui::panels::PlayerControlBar;
using shatv::ui::windows::MainWindow;

class LocalizationTest : public QObject {
    Q_OBJECT

   private slots:
    void zh_cn_translator_translates_core_ui_strings();
};

void LocalizationTest::zh_cn_translator_translates_core_ui_strings() {
    QTranslator translator;

    QVERIFY(translator.load(":/i18n/shatv_zh_CN.qm"));
    QVERIFY(qApp->installTranslator(&translator));
    QCOMPARE(PlaybackStateName(PlaybackState::kIdle), QString::fromUtf8("空闲"));
    QCOMPARE(MainWindow::tr("Ready"), QString::fromUtf8("就绪"));
    QCOMPARE(PlayerControlBar::tr("Play"), QString::fromUtf8("播放"));
    QCOMPARE(PlaybackStatusPanel::tr("%1 (retry %2)").arg(QString::fromUtf8("播放失败")).arg(2),
             QString::fromUtf8("播放失败（重试 2 次）"));
    qApp->removeTranslator(&translator);
}

}  // namespace

QTEST_GUILESS_MAIN(LocalizationTest)

#include "localization_test.moc"
