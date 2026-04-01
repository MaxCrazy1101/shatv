#pragma once

#include <QMetaType>
#include <QString>

namespace shatv::domain {

enum class PlaybackState : uint8_t {
    kIdle = 0,
    kLoading,
    kPlaying,
    kPaused,
    kBuffering,
    kRetrying,
    kError,
};

inline QString PlaybackStateName(PlaybackState state) {
    switch (state) {
        case PlaybackState::kIdle:
            return "Idle";
        case PlaybackState::kLoading:
            return "Loading";
        case PlaybackState::kPlaying:
            return "Playing";
        case PlaybackState::kPaused:
            return "Paused";
        case PlaybackState::kBuffering:
            return "Buffering";
        case PlaybackState::kRetrying:
            return "Retrying";
        case PlaybackState::kError:
            return "Error";
    }
    return "Unknown";
}

}  // namespace shatv::domain

Q_DECLARE_METATYPE(shatv::domain::PlaybackState)
