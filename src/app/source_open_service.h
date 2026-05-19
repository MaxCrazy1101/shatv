#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

#include "app/open_request.h"

class QNetworkAccessManager;

namespace shatv::app {

struct SourceOpenContext {
    QString current_directory;
    QString user_agent;
};

using OpenResolutionCallback = std::function<void(OpenResolution)>;

class SourceOpenService final : public QObject {
    Q_OBJECT

   public:
    explicit SourceOpenService(QNetworkAccessManager *network_manager, QObject *parent = nullptr);

    void Resolve(OpenRequest request, SourceOpenContext context, OpenResolutionCallback callback);

   private:
    void ResolveFilePath(const OpenRequest &request, const SourceOpenContext &context, domain::SourceOrigin origin,
                         OpenRequestKind recent_request_kind, OpenResolutionCallback callback);
    void ResolveUrlText(const OpenRequest &request, const SourceOpenContext &context, domain::SourceOrigin origin,
                        OpenRequestKind recent_request_kind, OpenResolutionCallback callback);
    void ResolveRemotePlaylist(const QUrl &url, const SourceOpenContext &context, RecentOpenItem recent_item,
                               int attempt, OpenResolutionCallback callback);
    OpenResolution ResolveLocalPlaylist(const QString &path, const SourceOpenContext &context,
                                        RecentOpenItem recent_item) const;
    OpenResolution ResolveDirectMedia(const domain::Channel &channel, const SourceOpenContext &context,
                                      domain::SourceOrigin origin, std::optional<RecentOpenItem> recent_item) const;

    QNetworkAccessManager *network_manager_ = nullptr;
};

}  // namespace shatv::app
