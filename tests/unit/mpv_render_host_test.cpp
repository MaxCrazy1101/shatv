#include <QtTest>

#include "player/mpv_player_backend.h"

namespace {

template <typename T>
concept HasRenderHostAttachment = requires(T backend) {
    backend.AttachRenderHost(nullptr);
    backend.DetachRenderHost();
};

class MpvRenderHostTest : public QObject {
    Q_OBJECT

   private slots:
    void backend_exposes_render_host_attachment_api();
};

void MpvRenderHostTest::backend_exposes_render_host_attachment_api() {
    QVERIFY2((HasRenderHostAttachment<shatv::player::MpvPlayerBackend>),
             "MpvPlayerBackend still only exposes widget-specific render attachment APIs");
}

}  // namespace

QTEST_GUILESS_MAIN(MpvRenderHostTest)
#include "mpv_render_host_test.moc"
