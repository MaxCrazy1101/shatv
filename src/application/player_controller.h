#pragma once

#include <QObject>

#include "domain/media_source.h"
#include "domain/player_snapshot.h"
#include "player/player_backend.h"

namespace shatv::application {

class PlayerController final : public QObject {
    Q_OBJECT

   public:
    explicit PlayerController(player::PlayerBackend *backend, QObject *parent = nullptr);

    void PlayResolvedChannel(const domain::ResolvedChannel &resolved_channel);
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
    void SpeechSubtitleChanged(const QString &text, bool is_final, qint64 latency_ms);
    void SpeechSubtitleCleared();

   private slots:
    void OnBackendSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot);

   private:
    player::PlayerBackend *backend_ = nullptr;
    domain::ResolvedChannel current_channel_;
    domain::PlayerSnapshot current_snapshot_;
};

}  // namespace shatv::application
