#pragma once

#include <QOpenGLWidget>

#include "domain/player_snapshot.h"
#include "player/mpv_render_host.h"

namespace shatv::player {

class MpvPlayerBackend;

class MpvRenderWidget final : public QOpenGLWidget, public MpvRenderHost {
    Q_OBJECT

   public:
    explicit MpvRenderWidget(QWidget *parent = nullptr);

    void ApplySnapshot(const domain::PlayerSnapshot &snapshot);
    void SetBackend(MpvPlayerBackend *backend);
    QOpenGLContext *CurrentContext() const override;
    void MakeCurrent() override;
    void DoneCurrent() override;
    void RequestUpdate() override;

   protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

   private:
    void PaintPlaceholder();

    MpvPlayerBackend *backend_ = nullptr;
    QString title_;
    QString subtitle_;
};

}  // namespace shatv::player
