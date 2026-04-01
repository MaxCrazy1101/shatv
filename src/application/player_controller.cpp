#include "application/player_controller.h"

namespace shatv::application {

PlayerController::PlayerController(player::PlayerBackend *backend, QObject *parent)
    : QObject(parent), backend_(backend) {
    Q_ASSERT(backend_ != nullptr);
    connect(backend_, &player::PlayerBackend::SnapshotChanged, this, &PlayerController::OnBackendSnapshotChanged);
}

void PlayerController::PlayChannel(const domain::Channel &channel) {
    current_channel_ = channel;
    backend_->Load(channel);
}

void PlayerController::Pause() {
    backend_->Pause();
}

void PlayerController::Resume() {
    backend_->Play();
}

void PlayerController::Stop() {
    backend_->Stop();
}

void PlayerController::SetVolume(int volume) {
    backend_->SetVolume(volume);
}

void PlayerController::SetMuted(bool muted) {
    backend_->SetMuted(muted);
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
