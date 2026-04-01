#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "domain/channel.h"

namespace shatv::app {

struct LaunchOptions {
    bool smoke_test = false;
    bool mpv_smoke = false;
    QString open_url_argument;
    QString open_media_argument;
};

LaunchOptions ParseLaunchOptions(const QStringList &arguments);
std::optional<domain::Channel> BuildStartupChannel(const LaunchOptions &options, const QString &smoke_media,
                                                   const QString &current_directory);

}  // namespace shatv::app
