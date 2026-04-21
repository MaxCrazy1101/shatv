#include "player/mpv_render_widget.h"

#include <QOpenGLContext>
#include <QPainter>

#include "domain/playback_state.h"
#include "player/mpv_player_backend.h"

namespace shatv::player {

MpvRenderWidget::MpvRenderWidget(QWidget *parent) : QOpenGLWidget(parent) {
    setMinimumHeight(320);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);

    domain::PlayerSnapshot initial_snapshot;
    initial_snapshot.state = domain::PlaybackState::kIdle;
    ApplySnapshot(initial_snapshot);
}

void MpvRenderWidget::ApplySnapshot(const domain::PlayerSnapshot &snapshot) {
    title_ = snapshot.channel_name.isEmpty() ? tr("No Channel Selected") : snapshot.channel_name;
    subtitle_ = tr("Stage 3 OpenGL Placeholder\nState: %1").arg(domain::PlaybackStateName(snapshot.state));
    update();
}

void MpvRenderWidget::SetBackend(MpvPlayerBackend *backend) {
    backend_ = backend;
}

QOpenGLContext *MpvRenderWidget::CurrentContext() const {
    return context();
}

void MpvRenderWidget::MakeCurrent() {
    makeCurrent();
}

void MpvRenderWidget::DoneCurrent() {
    doneCurrent();
}

void MpvRenderWidget::RequestUpdate() {
    update();
}

void MpvRenderWidget::initializeGL() {
    if (backend_ != nullptr) {
        backend_->InitializeRenderContext();
    }
}

void MpvRenderWidget::paintGL() {
    if (backend_ != nullptr &&
        backend_->RenderFrame(defaultFramebufferObject(), width(), height(), devicePixelRatioF())) {
        return;
    }

    PaintPlaceholder();
}

void MpvRenderWidget::resizeGL(int width, int height) {
    Q_UNUSED(width);
    Q_UNUSED(height);
    update();
}

void MpvRenderWidget::PaintPlaceholder() {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(10, 10, 10));
    painter.setPen(Qt::white);

    // 未接入可用视频帧时，继续保留占位绘制，便于 smoke 和错误态观察。
    painter.setFont(QFont("Sans Serif", 18, QFont::Bold));
    painter.drawText(rect().adjusted(24, 40, -24, -40), Qt::AlignHCenter | Qt::TextWordWrap, title_);

    painter.setFont(QFont("Sans Serif", 12));
    painter.drawText(rect().adjusted(24, 120, -24, -40), Qt::AlignHCenter | Qt::TextWordWrap, subtitle_);
}

}  // namespace shatv::player
