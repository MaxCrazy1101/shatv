#include <QtTest>

#include "player/mpv_render_widget.h"
#include "ui/widgets/playback_viewport.h"

namespace {

using shatv::ui::widgets::PlaybackViewport;

class PlaybackViewportTest : public QObject {
    Q_OBJECT

   private slots:
    void fullscreen_osd_hides_after_timeout();
    void mouse_move_reveals_hidden_osd();
};

void PlaybackViewportTest::fullscreen_osd_hides_after_timeout() {
    PlaybackViewport viewport;
    viewport.SetOsdAutoHideSeconds(1);
    viewport.resize(640, 360);
    viewport.show();

    viewport.SetFullscreenActive(true);
    QVERIFY(viewport.IsOsdVisible());

    QTest::qWait(1100);

    QVERIFY(!viewport.IsOsdVisible());
}

void PlaybackViewportTest::mouse_move_reveals_hidden_osd() {
    PlaybackViewport viewport;
    viewport.SetOsdAutoHideSeconds(1);
    viewport.resize(640, 360);
    viewport.show();

    viewport.SetFullscreenActive(true);
    QTest::qWait(1100);
    QVERIFY(!viewport.IsOsdVisible());

    QTest::mouseMove(viewport.RenderWidget(), QPoint(20, 20));

    QVERIFY(viewport.IsOsdVisible());
}

}  // namespace

QTEST_MAIN(PlaybackViewportTest)

#include "playback_viewport_test.moc"
