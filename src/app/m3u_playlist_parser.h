#pragma once

#include <vector>

#include <QString>

#include "domain/channel.h"

namespace shatv::app {

bool LooksLikeLocalM3uPath(const QString &path);
bool LooksLikeRemoteM3uUrl(const QUrl &url);
bool LooksLikePlaylistChannel(const domain::Channel &channel);
bool LooksLikeM3uPlaylistText(const QString &text);
std::vector<domain::Channel> ParseM3uPlaylistText(const QString &text, const QString &id_prefix);

}  // namespace shatv::app
