#pragma once

#include <QString>
#include <QStringList>
#include <optional>

#include "domain/channel.h"

namespace shatv::app {

struct LaunchOptions {
    bool ffmpeg_audio_smoke = false;
    bool ffmpeg_smoke = false;
    QString open_url_argument;
    QString open_media_argument;
};

LaunchOptions ParseLaunchOptions(const QStringList &arguments);
domain::Channel BuildOpenUrlChannel(const QString &input, const QString &current_directory);
domain::Channel BuildOpenMediaChannel(const QString &input, const QString &current_directory);
bool IsRemotePlaybackUrl(const QUrl &url);
bool LooksLikeRemoteMediaDirectoryUrl(const QUrl &url);
std::optional<domain::Channel> BuildStartupChannel(const LaunchOptions &options, const QString &current_directory);

}  // namespace shatv::app
