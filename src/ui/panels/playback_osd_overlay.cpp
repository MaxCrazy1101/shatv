#include "ui/panels/playback_osd_overlay.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "domain/playback_state.h"
#include "ui/panels/player_control_bar.h"

namespace shatv::ui::panels {

PlaybackOsdOverlay::PlaybackOsdOverlay(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(
        "PlaybackOsdOverlay {"
        "  background: rgba(0, 0, 0, 96);"
        "}"
        "QLabel { color: white; }"
        "QPushButton { min-height: 32px; }");

    title_label_ = new QLabel(tr("No Channel Selected"), this);
    state_label_ = new QLabel(tr("Idle"), this);
    exit_fullscreen_button_ = new QPushButton(tr("Exit Full Screen"), this);
    control_bar_ = new PlayerControlBar(this);

    title_label_->setStyleSheet("font-size: 18px; font-weight: 600;");
    state_label_->setStyleSheet("font-size: 12px;");

    auto *title_layout = new QVBoxLayout;
    title_layout->setContentsMargins(0, 0, 0, 0);
    title_layout->setSpacing(4);
    title_layout->addWidget(title_label_);
    title_layout->addWidget(state_label_);

    auto *header_layout = new QHBoxLayout;
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->addLayout(title_layout, 1);
    header_layout->addWidget(exit_fullscreen_button_);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);
    layout->addLayout(header_layout);
    layout->addStretch(1);
    layout->addWidget(control_bar_);

    connect(control_bar_, &PlayerControlBar::PlayPauseRequested, this, &PlaybackOsdOverlay::PlayPauseRequested);
    connect(control_bar_, &PlayerControlBar::StopRequested, this, &PlaybackOsdOverlay::StopRequested);
    connect(control_bar_, &PlayerControlBar::MuteToggled, this, &PlaybackOsdOverlay::MuteToggled);
    connect(control_bar_, &PlayerControlBar::VolumeChanged, this, &PlaybackOsdOverlay::VolumeChanged);
    connect(exit_fullscreen_button_, &QPushButton::clicked, this, &PlaybackOsdOverlay::ExitFullscreenRequested);
}

void PlaybackOsdOverlay::ApplySnapshot(const domain::PlayerSnapshot &snapshot) {
    title_label_->setText(snapshot.channel_name.isEmpty() ? tr("No Channel Selected") : snapshot.channel_name);
    state_label_->setText(domain::PlaybackStateName(snapshot.state));
    control_bar_->ApplySnapshot(snapshot);
}

}  // namespace shatv::ui::panels
