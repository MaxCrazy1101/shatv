#pragma once

#include <QObject>

#include "domain/channel.h"
#include "domain/player_snapshot.h"
#include "player/player_backend.h"

namespace shatv::application {

class PlayerController final : public QObject {
    Q_OBJECT

   public:
    explicit PlayerController(player::PlayerBackend *backend, QObject *parent = nullptr);

    void PlayChannel(const domain::Channel &channel);
    void Pause();
    void Resume();
    void Stop();
    void SetVolume(int volume);
    void SetMuted(bool muted);

    const domain::PlayerSnapshot &CurrentSnapshot() const;

   signals:
    void PlaybackSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot);
    void CurrentChannelChanged(const QString &channel_id);
    void TransientMessageChanged(const QString &message);

   private slots:
    void OnBackendSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot);

   private:
    player::PlayerBackend *backend_ = nullptr;
    domain::Channel current_channel_;
    domain::PlayerSnapshot current_snapshot_;
    int retry_count_ = 0;
    int retry_generation_ = 0;
    static constexpr int kMaxRetryCount = 1;
};

}  // namespace shatv::application
