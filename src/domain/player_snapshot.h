#pragma once

#include <QMetaType>
#include <QString>

#include "domain/playback_state.h"

namespace shatv::domain {

struct PlayerSnapshot {
    PlaybackState state = PlaybackState::kIdle;
    QString channel_id;
    QString channel_name;
    QString message;
    int volume = 50;
    bool muted = false;
    int retry_count = 0;
};

}  // namespace shatv::domain

Q_DECLARE_METATYPE(shatv::domain::PlayerSnapshot)
