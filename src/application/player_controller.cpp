#include "application/player_controller.h"

namespace shatv::application {

PlayerController::PlayerController(player::PlayerBackend *backend, QObject *parent)
    : QObject(parent), backend_(backend) {
    Q_ASSERT(backend_ != nullptr);
    connect(backend_, &player::PlayerBackend::SnapshotChanged, this, &PlayerController::OnBackendSnapshotChanged);
    connect(backend_, &player::PlayerBackend::SpeechSubtitleChanged, this, &PlayerController::SpeechSubtitleChanged);
    connect(backend_, &player::PlayerBackend::SpeechSubtitleCleared, this, &PlayerController::SpeechSubtitleCleared);
}

void PlayerController::PlayResolvedChannel(const domain::ResolvedChannel &resolved_channel) {
    current_channel_ = resolved_channel;
    current_snapshot_.retry_count = 0;
    if (!current_channel_.channel.id.isEmpty()) {
        emit CurrentChannelChanged(current_channel_.channel.id);
    }
    backend_->Load(resolved_channel.source);
}

void PlayerController::Pause() {
    backend_->Pause();
}

void PlayerController::Resume() {
    backend_->Play();
}

void PlayerController::Stop() {
    current_channel_ = {};
    current_snapshot_.retry_count = 0;
    backend_->Stop();
}

void PlayerController::SetVolume(int volume) {
    backend_->SetVolume(volume);
}

void PlayerController::SetMuted(bool muted) {
    backend_->SetMuted(muted);
}

void PlayerController::SetSpeechSubtitleEnabled(bool enabled) {
    backend_->SetSpeechSubtitleEnabled(enabled);
}

const domain::PlayerSnapshot &PlayerController::CurrentSnapshot() const {
    return current_snapshot_;
}

void PlayerController::OnBackendSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot) {
    // 控制器统一承接后端状态，再把应用层可理解的数据分发给 UI。
    current_snapshot_ = snapshot;

    emit PlaybackSnapshotChanged(current_snapshot_);
    if (!current_snapshot_.channel_id.isEmpty()) {
        emit CurrentChannelChanged(current_snapshot_.channel_id);
    }
    if (!current_snapshot_.message.isEmpty()) {
        emit TransientMessageChanged(current_snapshot_.message);
    }
}

}  // namespace shatv::application
