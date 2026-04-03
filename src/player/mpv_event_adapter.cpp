#include "player/mpv_event_adapter.h"

#include <QCoreApplication>

#include "domain/playback_state.h"

namespace shatv::player {

void MpvEventAdapter::ApplyFileLoaded(domain::PlayerSnapshot &snapshot, const QString &channel_name) const {
    snapshot.state = domain::PlaybackState::kPlaying;
    snapshot.channel_name = channel_name;
    snapshot.message = channel_name.isEmpty()
                           ? QCoreApplication::translate("MpvEventAdapter", "Playing")
                           : QCoreApplication::translate("MpvEventAdapter", "Playing %1").arg(channel_name);
}

void MpvEventAdapter::ApplyEndFileEof(domain::PlayerSnapshot &snapshot) const {
    snapshot.state = domain::PlaybackState::kIdle;
    snapshot.message = snapshot.channel_name.isEmpty()
                           ? QCoreApplication::translate("MpvEventAdapter", "Finished")
                           : QCoreApplication::translate("MpvEventAdapter", "Finished %1").arg(snapshot.channel_name);
}

void MpvEventAdapter::ApplyEofReached(domain::PlayerSnapshot &snapshot, bool eof_reached) const {
    if (!eof_reached || snapshot.channel_id.isEmpty()) {
        return;
    }

    ApplyEndFileEof(snapshot);
}

void MpvEventAdapter::ApplyIdleActive(domain::PlayerSnapshot &snapshot, bool idle_active) const {
    if (!idle_active || snapshot.channel_id.isEmpty()) {
        return;
    }

    ApplyEndFileEof(snapshot);
}

void MpvEventAdapter::ApplyPauseChanged(domain::PlayerSnapshot &snapshot, bool paused) const {
    if (snapshot.channel_id.isEmpty()) {
        return;
    }

    if (snapshot.state == domain::PlaybackState::kIdle) {
        return;
    }

    snapshot.state = paused ? domain::PlaybackState::kPaused : domain::PlaybackState::kPlaying;
    snapshot.message =
        snapshot.channel_name.isEmpty()
            ? (paused ? QCoreApplication::translate("MpvEventAdapter", "Paused")
                      : QCoreApplication::translate("MpvEventAdapter", "Playing"))
            : (paused ? QCoreApplication::translate("MpvEventAdapter", "Paused %1").arg(snapshot.channel_name)
                      : QCoreApplication::translate("MpvEventAdapter", "Playing %1").arg(snapshot.channel_name));
}

void MpvEventAdapter::ApplyEndFileError(domain::PlayerSnapshot &snapshot, const QString &message) const {
    snapshot.state = domain::PlaybackState::kError;
    snapshot.message = message;
}

}  // namespace shatv::player
