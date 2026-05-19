#pragma once

#include <QString>
#include <vector>

#include "domain/channel.h"

namespace shatv::app {

struct PlaylistImportResult {
    std::vector<domain::Channel> channels;
    QString epg_url;
};

bool LooksLikeLocalM3uPath(const QString &path);
bool LooksLikeRemoteM3uUrl(const QUrl &url);
bool LooksLikePlaylistChannel(const domain::Channel &channel);
bool LooksLikeM3uPlaylistText(const QString &text);
PlaylistImportResult ParsePlaylistImportText(const QString &text, const QString &id_prefix);
std::vector<domain::Channel> ParseM3uPlaylistText(const QString &text, const QString &id_prefix);

}  // namespace shatv::app
