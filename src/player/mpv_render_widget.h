#pragma once

#include <QOpenGLWidget>

#include "domain/player_snapshot.h"

namespace shatv::player {

class MpvPlayerBackend;

class MpvRenderWidget final : public QOpenGLWidget {
    Q_OBJECT

   public:
    explicit MpvRenderWidget(QWidget *parent = nullptr);

    void ApplySnapshot(const domain::PlayerSnapshot &snapshot);
    void SetBackend(MpvPlayerBackend *backend);

   protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

   private:
    void PaintPlaceholder();

    MpvPlayerBackend *backend_ = nullptr;
    QString title_ = "No Channel Selected";
    QString subtitle_ = "Stage 3 OpenGL Placeholder";
};

}  // namespace shatv::player
