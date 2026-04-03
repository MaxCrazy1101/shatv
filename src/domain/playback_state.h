#pragma once

#include <QCoreApplication>
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
            return QCoreApplication::translate("PlaybackState", "Idle");
        case PlaybackState::kLoading:
            return QCoreApplication::translate("PlaybackState", "Loading");
        case PlaybackState::kPlaying:
            return QCoreApplication::translate("PlaybackState", "Playing");
        case PlaybackState::kPaused:
            return QCoreApplication::translate("PlaybackState", "Paused");
        case PlaybackState::kBuffering:
            return QCoreApplication::translate("PlaybackState", "Buffering");
        case PlaybackState::kRetrying:
            return QCoreApplication::translate("PlaybackState", "Retrying");
        case PlaybackState::kError:
            return QCoreApplication::translate("PlaybackState", "Error");
    }
    return QCoreApplication::translate("PlaybackState", "Unknown");
}

}  // namespace shatv::domain

Q_DECLARE_METATYPE(shatv::domain::PlaybackState)
