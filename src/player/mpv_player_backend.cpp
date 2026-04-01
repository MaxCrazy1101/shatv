#include "player/mpv_player_backend.h"

#include <algorithm>

namespace shatv::player {

MpvPlayerBackend::MpvPlayerBackend(QObject *parent) : PlayerBackend(parent) {
    InitializeMpv();
}

MpvPlayerBackend::~MpvPlayerBackend() {
    if (handle_ != nullptr) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}

void MpvPlayerBackend::Load(const domain::Channel &channel) {
    current_channel_ = channel;
    EmitSnapshot(domain::PlaybackState::kLoading, QString("Loading %1").arg(channel.name));
    EmitSnapshot(domain::PlaybackState::kError, "mpv backend skeleton: render context not ready");
}

void MpvPlayerBackend::Play() {
    if (current_channel_.id.isEmpty()) {
        return;
    }
    EmitSnapshot(domain::PlaybackState::kError, "mpv backend skeleton: play not implemented yet");
}

void MpvPlayerBackend::Pause() {
    if (current_channel_.id.isEmpty()) {
        return;
    }
    EmitSnapshot(domain::PlaybackState::kPaused, "mpv backend skeleton: pause requested");
}

void MpvPlayerBackend::Stop() {
    EmitSnapshot(domain::PlaybackState::kIdle, "mpv backend skeleton: stop requested");
}

void MpvPlayerBackend::SetVolume(int volume) {
    volume_ = std::clamp(volume, 0, 100);
    EmitSnapshot(domain::PlaybackState::kIdle, QString("mpv backend skeleton: volume %1").arg(volume_));
}

void MpvPlayerBackend::SetMuted(bool muted) {
    muted_ = muted;
    EmitSnapshot(domain::PlaybackState::kIdle, muted_ ? "mpv backend skeleton: muted" : "mpv backend skeleton: unmuted");
}

void MpvPlayerBackend::EmitSnapshot(domain::PlaybackState state, const QString &message) {
    domain::PlayerSnapshot snapshot;
    snapshot.state = state;
    snapshot.channel_id = current_channel_.id;
    snapshot.channel_name = current_channel_.name;
    snapshot.message = message;
    snapshot.volume = volume_;
    snapshot.muted = muted_;

    emit SnapshotChanged(snapshot);
}

void MpvPlayerBackend::InitializeMpv() {
    handle_ = mpv_create();
    if (handle_ == nullptr) {
        return;
    }

    mpv_set_option_string(handle_, "config", "no");
    mpv_set_option_string(handle_, "terminal", "no");
    mpv_set_option_string(handle_, "vo", "libmpv");
    mpv_set_option_string(handle_, "hwdec", "no");

    if (mpv_initialize(handle_) < 0) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}

}  // namespace shatv::player
