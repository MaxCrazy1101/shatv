#pragma once

#include "player/player_backend.h"

namespace shatv::player {

class FakePlayerBackend final : public PlayerBackend {
    Q_OBJECT

   public:
    explicit FakePlayerBackend(QObject *parent = nullptr);

    void Load(const domain::Channel &channel) override;
    void Play() override;
    void Pause() override;
    void Stop() override;
    void SetVolume(int volume) override;
    void SetMuted(bool muted) override;

   private:
    domain::Channel current_channel_;
    domain::PlaybackState current_state_ = domain::PlaybackState::kIdle;
    int volume_ = 50;
    bool muted_ = false;

    void EmitSnapshot(domain::PlaybackState state, const QString &message);
};

}  // namespace shatv::player
