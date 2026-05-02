#pragma once

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include <QString>

#include "domain/media_source.h"

namespace shatv::app {

enum class OpenRequestKind : uint8_t {
    kFilePath = 0,
    kUrlText,
    kRecentItem,
    kStartupOpenMedia,
    kStartupOpenUrl,
};

struct RecentOpenItem {
    OpenRequestKind request_kind = OpenRequestKind::kFilePath;
    QString target;
    QString label;
};

struct OpenRequest {
    OpenRequestKind request_kind = OpenRequestKind::kFilePath;
    QString target;
    QString label;
    std::optional<OpenRequestKind> replay_request_kind;
};

struct DirectMediaResolution {
    domain::ResolvedChannel item;
    std::optional<RecentOpenItem> recent_item;
};

struct ChannelListResolution {
    std::vector<domain::ResolvedChannel> channels;
    std::optional<RecentOpenItem> recent_item;
    QString playlist_epg_url;
};

struct OpenErrorResolution {
    QString message;
};

using OpenResolution = std::variant<DirectMediaResolution, ChannelListResolution, OpenErrorResolution>;

QString OpenRequestKindToken(OpenRequestKind request_kind);
std::optional<OpenRequestKind> OpenRequestKindFromToken(const QString &token);
std::optional<OpenRequestKind> LegacyOpenRequestKindFromToken(const QString &token);

}  // namespace shatv::app
