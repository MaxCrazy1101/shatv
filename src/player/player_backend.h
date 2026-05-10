#pragma once

#include <QObject>
#include <QString>
#include <QtGlobal>

#include "domain/media_source.h"
#include "domain/player_snapshot.h"

namespace shatv::player {

class PlayerBackend : public QObject {
    Q_OBJECT

   public:
    explicit PlayerBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~PlayerBackend() override = default;

    virtual void Load(const domain::MediaSourceDescriptor &source) = 0;
    virtual void Play() = 0;
    virtual void Pause() = 0;
    virtual void Stop() = 0;
    virtual void SetVolume(int volume) = 0;
    virtual void SetMuted(bool muted) = 0;
    virtual void SetSpeechSubtitleEnabled(bool enabled) = 0;

   signals:
    void SnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot);
    void SpeechSubtitleChanged(const QString &text, bool is_final, qint64 latency_ms);
    void SpeechSubtitleCleared();
};

}  // namespace shatv::player
