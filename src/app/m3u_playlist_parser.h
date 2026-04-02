#pragma once

#include <vector>

#include <QString>

#include "domain/channel.h"

namespace shatv::app {

bool LooksLikeM3uPlaylistText(const QString &text);
std::vector<domain::Channel> ParseM3uPlaylistText(const QString &text, const QString &id_prefix);

}  // namespace shatv::app
