#include "player/fake_player_backend.h"

#include <algorithm>

#include <QTimer>

namespace shatv::player {

FakePlayerBackend::FakePlayerBackend(QObject *parent) : PlayerBackend(parent) {}

void FakePlayerBackend::Load(const domain::Channel &channel) {
    current_channel_ = channel;
    EmitSnapshot(domain::PlaybackState::kLoading, QString("Loading %1").arg(channel.name));

    // 用异步回调模拟真实播放器加载完成后的状态回流。
    QTimer::singleShot(0, this, [this]() { EmitSnapshot(domain::PlaybackState::kPlaying, "Fake backend is playing"); });
}

void FakePlayerBackend::Play() {
    if (current_channel_.id.isEmpty()) {
        return;
    }
    EmitSnapshot(domain::PlaybackState::kPlaying, "Playback resumed");
}

void FakePlayerBackend::Pause() {
    if (current_channel_.id.isEmpty()) {
        return;
    }
    EmitSnapshot(domain::PlaybackState::kPaused, "Playback paused");
}

void FakePlayerBackend::Stop() {
    EmitSnapshot(domain::PlaybackState::kIdle, "Playback stopped");
}

void FakePlayerBackend::SetVolume(int volume) {
    volume_ = std::clamp(volume, 0, 100);
    EmitSnapshot(current_state_, QString("Volume %1").arg(volume_));
}

void FakePlayerBackend::SetMuted(bool muted) {
    muted_ = muted;
    EmitSnapshot(current_state_, muted_ ? "Muted" : "Unmuted");
}

void FakePlayerBackend::EmitSnapshot(domain::PlaybackState state, const QString &message) {
    current_state_ = state;

    domain::PlayerSnapshot snapshot;
    snapshot.state = state;
    snapshot.channel_id = current_channel_.id;
    snapshot.channel_name = current_channel_.name;
    snapshot.message = message;
    snapshot.volume = volume_;
    snapshot.muted = muted_;

    emit SnapshotChanged(snapshot);
}

}  // namespace shatv::player
