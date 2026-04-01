#include "ui/panels/player_control_bar.h"

#include <QHBoxLayout>

#include "domain/playback_state.h"

namespace shatv::ui::panels {

PlayerControlBar::PlayerControlBar(QWidget *parent) : QWidget(parent) {
    play_pause_button_ = new QPushButton("Play", this);
    stop_button_ = new QPushButton("Stop", this);
    mute_button_ = new QPushButton("Mute", this);
    volume_slider_ = new QSlider(Qt::Horizontal, this);
    volume_slider_->setRange(0, 100);
    volume_slider_->setValue(50);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(play_pause_button_);
    layout->addWidget(stop_button_);
    layout->addWidget(mute_button_);
    layout->addWidget(volume_slider_, 1);

    connect(play_pause_button_, &QPushButton::clicked, this, &PlayerControlBar::PlayPauseRequested);
    connect(stop_button_, &QPushButton::clicked, this, &PlayerControlBar::StopRequested);
    connect(mute_button_, &QPushButton::clicked, this, [this]() {
        muted_ = !muted_;
        emit MuteToggled(muted_);
    });
    connect(volume_slider_, &QSlider::valueChanged, this, &PlayerControlBar::VolumeChanged);
}

void PlayerControlBar::ApplySnapshot(const domain::PlayerSnapshot &snapshot) {
    play_pause_button_->setText(snapshot.state == domain::PlaybackState::kPlaying ? "Pause" : "Play");

    muted_ = snapshot.muted;
    mute_button_->setText(muted_ ? "Unmute" : "Mute");

    const bool volume_changed = volume_slider_->value() != snapshot.volume;
    if (volume_changed) {
        volume_slider_->blockSignals(true);
        volume_slider_->setValue(snapshot.volume);
        volume_slider_->blockSignals(false);
    }
}

}  // namespace shatv::ui::panels
