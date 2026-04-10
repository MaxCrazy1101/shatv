#include <QtTest>

#include "application/player_controller.h"
#include "player/fake_player_backend.h"
#include "ui/models/channel_list_model.h"
#include "ui/panels/playback_status_panel.h"
#include "ui/panels/player_control_bar.h"
#include "ui/windows/main_window.h"

namespace {

using shatv::application::PlayerController;
using shatv::player::FakePlayerBackend;
using shatv::ui::models::ChannelListModel;
using shatv::ui::panels::PlaybackStatusPanel;
using shatv::ui::panels::PlayerControlBar;
using shatv::ui::windows::MainWindow;

void SendKey(QWidget *widget, int key) {
    QKeyEvent press_event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &press_event);
    QKeyEvent release_event(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &release_event);
}

class MainWindowFullscreenTest : public QObject {
    Q_OBJECT

   private slots:
    void f11_enters_fullscreen_and_hides_non_video_ui();
    void escape_exits_fullscreen_and_restores_ui();
};

void MainWindowFullscreenTest::f11_enters_fullscreen_and_hides_non_video_ui() {
    FakePlayerBackend backend;
    PlayerController controller(&backend);
    ChannelListModel channel_model;
    MainWindow window(&controller, &channel_model);

    window.resize(1280, 720);
    window.show();

    SendKey(&window, Qt::Key_F11);

    QTRY_VERIFY(window.IsFullscreenModeActive());
    QVERIFY(window.findChild<PlayerControlBar *>()->isHidden());
    QVERIFY(window.findChild<PlaybackStatusPanel *>()->isHidden());
    QVERIFY(window.findChild<QListView *>()->parentWidget()->isHidden());
}

void MainWindowFullscreenTest::escape_exits_fullscreen_and_restores_ui() {
    FakePlayerBackend backend;
    PlayerController controller(&backend);
    ChannelListModel channel_model;
    MainWindow window(&controller, &channel_model);

    window.resize(1280, 720);
    window.show();

    SendKey(&window, Qt::Key_F11);
    QTRY_VERIFY(window.IsFullscreenModeActive());

    SendKey(&window, Qt::Key_Escape);

    QTRY_VERIFY(!window.IsFullscreenModeActive());
    QVERIFY(window.findChild<PlayerControlBar *>()->isVisible());
    QVERIFY(window.findChild<PlaybackStatusPanel *>()->isVisible());
    QVERIFY(window.findChild<QListView *>()->parentWidget()->isVisible());
}

}  // namespace

QTEST_MAIN(MainWindowFullscreenTest)

#include "main_window_fullscreen_test.moc"
