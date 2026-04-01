#pragma once

#include <QString>

#include "domain/player_snapshot.h"

namespace shatv::player {

class MpvEventAdapter final {
   public:
    void ApplyFileLoaded(domain::PlayerSnapshot &snapshot, const QString &channel_name) const;
    void ApplyEndFileEof(domain::PlayerSnapshot &snapshot) const;
    void ApplyPauseChanged(domain::PlayerSnapshot &snapshot, bool paused) const;
    void ApplyEndFileError(domain::PlayerSnapshot &snapshot, const QString &message) const;
};

}  // namespace shatv::player
