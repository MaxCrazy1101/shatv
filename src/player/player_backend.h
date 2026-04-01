#pragma once

#include <QObject>

#include "domain/channel.h"
#include "domain/player_snapshot.h"

namespace shatv::player {

class PlayerBackend : public QObject {
    Q_OBJECT

   public:
    explicit PlayerBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~PlayerBackend() override = default;

    virtual void Load(const domain::Channel &channel) = 0;
    virtual void Play() = 0;
    virtual void Pause() = 0;
    virtual void Stop() = 0;
    virtual void SetVolume(int volume) = 0;
    virtual void SetMuted(bool muted) = 0;

   signals:
    void SnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot);
};

}  // namespace shatv::player
