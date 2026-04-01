#pragma once

#include <mpv/client.h>

#include "player/player_backend.h"

namespace shatv::player {

class MpvPlayerBackend final : public PlayerBackend {
    Q_OBJECT

   public:
    explicit MpvPlayerBackend(QObject *parent = nullptr);
    ~MpvPlayerBackend() override;

    void Load(const domain::Channel &channel) override;
    void Play() override;
    void Pause() override;
    void Stop() override;
    void SetVolume(int volume) override;
    void SetMuted(bool muted) override;

   private:
    domain::Channel current_channel_;
    int volume_ = 50;
    bool muted_ = false;
    mpv_handle *handle_ = nullptr;

    void EmitSnapshot(domain::PlaybackState state, const QString &message);
    void InitializeMpv();
};

}  // namespace shatv::player
