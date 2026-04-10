#include "ui/widgets/playback_viewport.h"

#include <QEvent>
#include <QStackedLayout>

#include "player/mpv_render_widget.h"
#include "ui/panels/playback_osd_overlay.h"

namespace shatv::ui::widgets {

PlaybackViewport::PlaybackViewport(QWidget *parent) : QWidget(parent) {
    render_widget_ = new player::MpvRenderWidget(this);
    osd_overlay_ = new panels::PlaybackOsdOverlay(this);
    osd_hide_timer_ = new QTimer(this);
    osd_hide_timer_->setSingleShot(true);

    auto *layout = new QStackedLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setStackingMode(QStackedLayout::StackAll);
    layout->addWidget(render_widget_);
    layout->addWidget(osd_overlay_);

    setMouseTracking(true);
    render_widget_->setMouseTracking(true);
    osd_overlay_->hide();

    RegisterActivitySource(render_widget_);
    RegisterActivitySource(osd_overlay_);

    connect(osd_hide_timer_, &QTimer::timeout, this, &PlaybackViewport::HideOsd);
    connect(osd_overlay_, &panels::PlaybackOsdOverlay::PlayPauseRequested, this,
            &PlaybackViewport::PlayPauseRequested);
    connect(osd_overlay_, &panels::PlaybackOsdOverlay::StopRequested, this, &PlaybackViewport::StopRequested);
    connect(osd_overlay_, &panels::PlaybackOsdOverlay::MuteToggled, this, &PlaybackViewport::MuteToggled);
    connect(osd_overlay_, &panels::PlaybackOsdOverlay::VolumeChanged, this, &PlaybackViewport::VolumeChanged);
    connect(osd_overlay_, &panels::PlaybackOsdOverlay::ExitFullscreenRequested, this,
            &PlaybackViewport::ExitFullscreenRequested);
}

void PlaybackViewport::ApplySnapshot(const domain::PlayerSnapshot &snapshot) {
    render_widget_->ApplySnapshot(snapshot);
    osd_overlay_->ApplySnapshot(snapshot);
}

void PlaybackViewport::SetFullscreenActive(bool active) {
    if (fullscreen_active_ == active) {
        return;
    }

    fullscreen_active_ = active;
    if (!fullscreen_active_) {
        osd_hide_timer_->stop();
        osd_overlay_->hide();
        return;
    }

    RevealOsd();
}

void PlaybackViewport::SetOsdAutoHideSeconds(int seconds) {
    Q_ASSERT(seconds >= 1);
    if (seconds >= 1) {
        osd_auto_hide_seconds_ = seconds;
    }
}

void PlaybackViewport::RevealOsd() {
    if (!fullscreen_active_) {
        return;
    }

    osd_overlay_->show();
    osd_overlay_->raise();
    RestartHideTimer();
}

bool PlaybackViewport::IsOsdVisible() const {
    return osd_overlay_->isVisible();
}

player::MpvRenderWidget *PlaybackViewport::RenderWidget() const {
    return render_widget_;
}

panels::PlaybackOsdOverlay *PlaybackViewport::OsdOverlay() const {
    return osd_overlay_;
}

bool PlaybackViewport::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched);

    if (fullscreen_active_) {
        switch (event->type()) {
            case QEvent::MouseMove:
            case QEvent::MouseButtonPress:
                RevealOsd();
                break;
            default:
                break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void PlaybackViewport::RegisterActivitySource(QWidget *widget) {
    widget->setMouseTracking(true);
    widget->installEventFilter(this);

    const auto children = widget->findChildren<QWidget *>();
    for (QWidget *child : children) {
        child->setMouseTracking(true);
        child->installEventFilter(this);
    }
}

void PlaybackViewport::RestartHideTimer() {
    osd_hide_timer_->start(osd_auto_hide_seconds_ * 1000);
}

void PlaybackViewport::HideOsd() {
    osd_overlay_->hide();
}

}  // namespace shatv::ui::widgets
