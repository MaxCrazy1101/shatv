#include "application/player_controller.h"

#include <QTimer>

namespace shatv::application {

PlayerController::PlayerController(player::PlayerBackend *backend, QObject *parent)
    : QObject(parent), backend_(backend) {
    Q_ASSERT(backend_ != nullptr);
    connect(backend_, &player::PlayerBackend::SnapshotChanged, this, &PlayerController::OnBackendSnapshotChanged);
}

void PlayerController::PlayChannel(const domain::Channel &channel) {
    ++retry_generation_;
    current_channel_ = channel;
    retry_count_ = 0;
    current_snapshot_.retry_count = 0;
    backend_->Load(channel);
}

void PlayerController::Pause() {
    backend_->Pause();
}

void PlayerController::Resume() {
    backend_->Play();
}

void PlayerController::Stop() {
    ++retry_generation_;
    current_channel_ = {};
    retry_count_ = 0;
    current_snapshot_.retry_count = 0;
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
    if (snapshot.state == domain::PlaybackState::kError && !current_channel_.id.isEmpty() &&
        retry_count_ < kMaxRetryCount) {
        const domain::Channel retry_channel = current_channel_;
        const int retry_generation = retry_generation_;
        ++retry_count_;

        current_snapshot_ = snapshot;
        current_snapshot_.state = domain::PlaybackState::kRetrying;
        current_snapshot_.retry_count = retry_count_;

        emit PlaybackSnapshotChanged(current_snapshot_);
        emit CurrentChannelChanged(current_channel_.id);
        if (!current_snapshot_.message.isEmpty()) {
            emit TransientMessageChanged(current_snapshot_.message);
        }

        // 控制器只做一次有界自动重试，避免把重试策略塞进播放器后端。
        QTimer::singleShot(300, this, [this, retry_channel, retry_generation]() {
            if (retry_generation_ != retry_generation || current_channel_.id != retry_channel.id) {
                return;
            }
            backend_->Load(retry_channel);
        });
        return;
    }

    // 控制器统一承接后端状态，再把应用层可理解的数据分发给 UI。
    current_snapshot_ = snapshot;
    if (snapshot.state == domain::PlaybackState::kPlaying) {
        retry_count_ = 0;
    }
    current_snapshot_.retry_count = retry_count_;

    emit PlaybackSnapshotChanged(current_snapshot_);
    if (!current_snapshot_.channel_id.isEmpty()) {
        emit CurrentChannelChanged(current_snapshot_.channel_id);
    }
    if (!current_snapshot_.message.isEmpty()) {
        emit TransientMessageChanged(current_snapshot_.message);
    }
}

}  // namespace shatv::application
