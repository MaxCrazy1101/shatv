#include "ui/qml_spike/spike_playback_bridge.h"

#include "application/player_controller.h"
#include "domain/playback_state.h"
#include "ui/qml_spike/mpv_video_item.h"

namespace shatv::ui::qml_spike {

SpikePlaybackBridge::SpikePlaybackBridge(QObject *parent) : QObject(parent) {}

void SpikePlaybackBridge::SetController(application::PlayerController *controller) {
    controller_ = controller;
    if (controller_ == nullptr) {
        return;
    }

    // Spike 阶段仍沿用现有应用层控制器，避免把控制语义重复塞进 QML。
    connect(controller_, &application::PlayerController::PlaybackSnapshotChanged, this,
            &SpikePlaybackBridge::ApplySnapshot);
    ApplySnapshot(controller_->CurrentSnapshot());
}

void SpikePlaybackBridge::SetVideoItem(MpvVideoItem *video_item) {
    video_item_ = video_item;
    if (video_item_ == nullptr) {
        return;
    }

    connect(video_item_, &MpvVideoItem::readyChanged, this, [this]() {
        video_ready_ = video_item_ != nullptr && video_item_->ready();
        emit videoReadyChanged();
    });
    video_ready_ = video_item_->ready();
    emit videoReadyChanged();
}

void SpikePlaybackBridge::SetStartupChannel(const std::optional<domain::Channel> &channel) {
    source_label_ = channel.has_value() ? channel->url.toString() : tr("No startup media");
    emit sourceLabelChanged();
}

QString SpikePlaybackBridge::statusMessage() const {
    return snapshot_.message;
}

QString SpikePlaybackBridge::sourceLabel() const {
    return source_label_;
}

QString SpikePlaybackBridge::playbackState() const {
    return domain::PlaybackStateName(snapshot_.state);
}

bool SpikePlaybackBridge::videoReady() const {
    return video_ready_;
}

void SpikePlaybackBridge::togglePlayPause() {
    if (controller_ == nullptr) {
        return;
    }

    if (snapshot_.state == domain::PlaybackState::kPlaying || snapshot_.state == domain::PlaybackState::kLoading) {
        controller_->Pause();
        return;
    }

    controller_->Resume();
}

void SpikePlaybackBridge::stop() {
    if (controller_ == nullptr) {
        return;
    }

    controller_->Stop();
}

void SpikePlaybackBridge::ApplySnapshot(const domain::PlayerSnapshot &snapshot) {
    const bool state_changed = snapshot_.state != snapshot.state;
    const bool message_changed = snapshot_.message != snapshot.message;

    snapshot_ = snapshot;
    if (state_changed) {
        emit playbackStateChanged();
    }
    if (message_changed) {
        emit statusMessageChanged();
    }
}

}  // namespace shatv::ui::qml_spike
