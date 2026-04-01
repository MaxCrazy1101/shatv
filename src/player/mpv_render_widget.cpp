#include "player/mpv_render_widget.h"

#include <QPainter>
#include <QPaintEvent>

#include "domain/playback_state.h"

namespace shatv::player {

MpvRenderWidget::MpvRenderWidget(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(320);
    setAutoFillBackground(false);
}

void MpvRenderWidget::ApplySnapshot(const domain::PlayerSnapshot &snapshot) {
    title_ = snapshot.channel_name.isEmpty() ? "No Channel Selected" : snapshot.channel_name;
    subtitle_ = QString("Stage 2 Video Placeholder\nState: %1").arg(domain::PlaybackStateName(snapshot.state));
    update();
}

void MpvRenderWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(10, 10, 10));
    painter.setPen(Qt::white);

    // 阶段 2 先提供稳定的占位绘制区域，阶段 3 再切换为真实 mpv 渲染。
    painter.setFont(QFont("Sans Serif", 18, QFont::Bold));
    painter.drawText(rect().adjusted(24, 40, -24, -40), Qt::AlignHCenter | Qt::TextWordWrap, title_);

    painter.setFont(QFont("Sans Serif", 12));
    painter.drawText(rect().adjusted(24, 120, -24, -40), Qt::AlignHCenter | Qt::TextWordWrap, subtitle_);

    Q_UNUSED(event);
}

}  // namespace shatv::player
