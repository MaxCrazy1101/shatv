#include "app/open_request.h"

#include <QtGlobal>

namespace shatv::app {

QString OpenRequestKindToken(OpenRequestKind request_kind) {
    switch (request_kind) {
        case OpenRequestKind::kFilePath:
            return QStringLiteral("file_path");
        case OpenRequestKind::kUrlText:
            return QStringLiteral("url_text");
        case OpenRequestKind::kRecentItem:
            return QStringLiteral("recent_item");
        case OpenRequestKind::kStartupOpenMedia:
            return QStringLiteral("startup_open_media");
        case OpenRequestKind::kStartupOpenUrl:
            return QStringLiteral("startup_open_url");
    }
    Q_UNREACHABLE_RETURN(QString());
}

std::optional<OpenRequestKind> OpenRequestKindFromToken(const QString &token) {
    if (token == QStringLiteral("file_path")) {
        return OpenRequestKind::kFilePath;
    }
    if (token == QStringLiteral("url_text")) {
        return OpenRequestKind::kUrlText;
    }
    if (token == QStringLiteral("recent_item")) {
        return OpenRequestKind::kRecentItem;
    }
    if (token == QStringLiteral("startup_open_media")) {
        return OpenRequestKind::kStartupOpenMedia;
    }
    if (token == QStringLiteral("startup_open_url")) {
        return OpenRequestKind::kStartupOpenUrl;
    }
    return std::nullopt;
}

std::optional<OpenRequestKind> LegacyOpenRequestKindFromToken(const QString &token) {
    if (token == QStringLiteral("file")) {
        return OpenRequestKind::kFilePath;
    }
    if (token == QStringLiteral("url")) {
        return OpenRequestKind::kUrlText;
    }
    return OpenRequestKindFromToken(token);
}

}  // namespace shatv::app
