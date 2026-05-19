#include "app/source_open_service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QtGlobal>
#include <memory>

#include "app/launch_options.h"
#include "app/logging.h"
#include "app/m3u_playlist_parser.h"

namespace shatv::app {

namespace {

constexpr int kNetworkTransferTimeoutMillis = 30000;

bool SupportsHttpHeaders(const QUrl &url) {
    if (!url.isValid() || url.isLocalFile()) {
        return false;
    }

    const QString scheme = url.scheme().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
}

QString MediaUserAgent(const QUrl &url, const SourceOpenContext &context) {
    return SupportsHttpHeaders(url) ? context.user_agent : QString();
}

QString NormalizedPlaylistPrefix(const QString &prefix) {
    return prefix.isEmpty() ? QStringLiteral("playlist") : prefix;
}

RecentOpenItem BuildRecentItem(OpenRequestKind request_kind, const domain::Channel &channel) {
    const QString target =
        channel.url.isLocalFile() ? QFileInfo(channel.url.toLocalFile()).absoluteFilePath() : channel.url.toString();
    return RecentOpenItem{
        .request_kind = request_kind,
        .target = target,
        .label = channel.name.isEmpty() ? target : channel.name,
    };
}

domain::MediaSourceDescriptor BuildDescriptor(const domain::Channel &channel, domain::SourceKind source_kind,
                                              domain::SourceOrigin origin, const SourceOpenContext &context) {
    return domain::MediaSourceDescriptor{
        .id = channel.id,
        .name = channel.name,
        .url = channel.url,
        .source_kind = source_kind,
        .origin = origin,
        .user_agent = MediaUserAgent(channel.url, context),
        .retry_policy = domain::RetryPolicyForSourceKind(source_kind),
    };
}

std::vector<domain::ResolvedChannel> BuildResolvedChannels(const std::vector<domain::Channel> &channels,
                                                           domain::SourceOrigin origin,
                                                           const SourceOpenContext &context) {
    std::vector<domain::ResolvedChannel> resolved_channels;
    resolved_channels.reserve(channels.size());
    for (const domain::Channel &channel : channels) {
        resolved_channels.push_back(domain::ResolvedChannel{
            .channel = channel,
            .source = BuildDescriptor(channel, domain::SourceKind::kPlaylistChannelLive, origin, context),
        });
    }
    return resolved_channels;
}

domain::SourceOrigin OriginForRequest(OpenRequestKind request_kind) {
    switch (request_kind) {
        case OpenRequestKind::kFilePath:
            return domain::SourceOrigin::kManualOpenFile;
        case OpenRequestKind::kUrlText:
            return domain::SourceOrigin::kManualOpenUrl;
        case OpenRequestKind::kRecentItem:
            return domain::SourceOrigin::kRecentItem;
        case OpenRequestKind::kStartupOpenMedia:
        case OpenRequestKind::kStartupOpenUrl:
            return domain::SourceOrigin::kStartupArgument;
    }
    Q_UNREACHABLE_RETURN(domain::SourceOrigin::kManualOpenFile);
}

OpenRequestKind RecentRequestKindFor(OpenRequestKind request_kind) {
    switch (request_kind) {
        case OpenRequestKind::kStartupOpenMedia:
            return OpenRequestKind::kFilePath;
        case OpenRequestKind::kStartupOpenUrl:
            return OpenRequestKind::kUrlText;
        case OpenRequestKind::kRecentItem:
            return OpenRequestKind::kRecentItem;
        case OpenRequestKind::kFilePath:
        case OpenRequestKind::kUrlText:
            return request_kind;
    }
    Q_UNREACHABLE_RETURN(OpenRequestKind::kFilePath);
}

OpenErrorResolution BuildError(const QString &message) {
    return OpenErrorResolution{
        .message = message,
    };
}

}  // namespace

SourceOpenService::SourceOpenService(QNetworkAccessManager *network_manager, QObject *parent)
    : QObject(parent), network_manager_(network_manager) {
    Q_ASSERT(network_manager_ != nullptr);
}

void SourceOpenService::Resolve(OpenRequest request, SourceOpenContext context, OpenResolutionCallback callback) {
    if (!callback) {
        return;
    }

    if (request.target.trimmed().isEmpty()) {
        qCWarning(log_app) << "Open resolver rejected empty target";
        callback(BuildError(QCoreApplication::translate("SourceOpenService", "Open request is empty")));
        return;
    }

    OpenRequestKind effective_kind = request.request_kind;
    OpenRequestKind recent_request_kind = RecentRequestKindFor(request.request_kind);
    if (request.request_kind == OpenRequestKind::kRecentItem) {
        if (!request.replay_request_kind.has_value()) {
            qCWarning(log_app) << "Open resolver rejected recent item without replay kind";
            callback(BuildError(QCoreApplication::translate("SourceOpenService", "Recent item cannot be reopened")));
            return;
        }
        effective_kind = *request.replay_request_kind;
        recent_request_kind = effective_kind;
    }

    const domain::SourceOrigin origin = OriginForRequest(request.request_kind);
    switch (effective_kind) {
        case OpenRequestKind::kFilePath:
        case OpenRequestKind::kStartupOpenMedia:
            ResolveFilePath(request, context, origin, recent_request_kind, std::move(callback));
            return;
        case OpenRequestKind::kUrlText:
        case OpenRequestKind::kStartupOpenUrl:
            ResolveUrlText(request, context, origin, recent_request_kind, std::move(callback));
            return;
        case OpenRequestKind::kRecentItem:
            callback(BuildError(QCoreApplication::translate("SourceOpenService", "Recent item cannot be reopened")));
            return;
    }
}

void SourceOpenService::ResolveFilePath(const OpenRequest &request, const SourceOpenContext &context,
                                        domain::SourceOrigin origin, OpenRequestKind recent_request_kind,
                                        OpenResolutionCallback callback) {
    if (LooksLikeLocalM3uPath(request.target)) {
        const domain::Channel channel = BuildOpenMediaChannel(request.target, context.current_directory);
        callback(
            ResolveLocalPlaylist(channel.url.toLocalFile(), context, BuildRecentItem(recent_request_kind, channel)));
        return;
    }

    const domain::Channel channel = BuildOpenMediaChannel(request.target, context.current_directory);
    callback(ResolveDirectMedia(channel, context, origin, BuildRecentItem(recent_request_kind, channel)));
}

void SourceOpenService::ResolveUrlText(const OpenRequest &request, const SourceOpenContext &context,
                                       domain::SourceOrigin origin, OpenRequestKind recent_request_kind,
                                       OpenResolutionCallback callback) {
    const domain::Channel channel = BuildOpenUrlChannel(request.target, context.current_directory);
    if (!IsRemotePlaybackUrl(channel.url)) {
        qCWarning(log_app).noquote() << "Open URL rejected target=" << RedactUrlForLog(channel.url);
        callback(BuildError(
            QCoreApplication::translate("SourceOpenService", "Open Link expects an http:// or https:// URL.")));
        return;
    }
    if (LooksLikeRemoteMediaDirectoryUrl(channel.url)) {
        qCWarning(log_app).noquote() << "Open URL rejected directory target=" << RedactUrlForLog(channel.url);
        callback(BuildError(QCoreApplication::translate(
            "SourceOpenService", "Open Link needs a full media URL, for example http://127.0.0.1:8080/index.m3u8")));
        return;
    }
    if (LooksLikeRemoteM3uUrl(channel.url)) {
        ResolveRemotePlaylist(channel.url, context, BuildRecentItem(recent_request_kind, channel), 1,
                              std::move(callback));
        return;
    }

    callback(ResolveDirectMedia(channel, context, origin, BuildRecentItem(recent_request_kind, channel)));
}

void SourceOpenService::ResolveRemotePlaylist(const QUrl &url, const SourceOpenContext &context,
                                              RecentOpenItem recent_item, int attempt,
                                              OpenResolutionCallback callback) {
    QNetworkRequest request(url);
    request.setTransferTimeout(kNetworkTransferTimeoutMillis);
    if (!context.user_agent.isEmpty()) {
        request.setHeader(QNetworkRequest::UserAgentHeader, context.user_agent);
    }

    qCInfo(log_network).noquote() << "Remote playlist fetch started"
                                  << "url=" << RedactUrlForLog(url) << "attempt=" << attempt;
    QNetworkReply *reply = network_manager_->get(request);
    QObject::connect(
        reply, &QNetworkReply::finished, this,
        [this, reply, url, context, recent_item = std::move(recent_item), attempt,
         callback = std::move(callback)]() mutable {
            const std::unique_ptr<QNetworkReply, void (*)(QNetworkReply *)> cleanup(
                reply, [](QNetworkReply *reply_to_delete) {
                    if (reply_to_delete != nullptr) {
                        reply_to_delete->deleteLater();
                    }
                });

            const domain::RetryPolicy fetch_policy =
                domain::RetryPolicyForSourceKind(domain::SourceKind::kRemotePlaylistFetch);
            if (reply->error() != QNetworkReply::NoError) {
                qCWarning(log_network).noquote()
                    << "Remote playlist fetch failed"
                    << "url=" << RedactUrlForLog(url)
                    << "status=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                    << "attempt=" << attempt << "reason=" << reply->errorString();
                if (attempt < fetch_policy.max_attempts) {
                    QTimer::singleShot(fetch_policy.initial_delay_ms, this,
                                       [this, url, context, recent_item, attempt, callback]() mutable {
                                           ResolveRemotePlaylist(url, context, recent_item, attempt + 1,
                                                                 std::move(callback));
                                       });
                    return;
                }
                callback(BuildError(QCoreApplication::translate("SourceOpenService", "Failed to download playlist")));
                return;
            }

            const QString text = QString::fromUtf8(reply->readAll());
            if (!LooksLikeM3uPlaylistText(text)) {
                qCWarning(log_network).noquote() << "Remote playlist unsupported format url=" << RedactUrlForLog(url);
                callback(
                    BuildError(QCoreApplication::translate("SourceOpenService", "Playlist format is not supported")));
                return;
            }

            const QString prefix = NormalizedPlaylistPrefix(QFileInfo(url.path()).baseName());
            const PlaylistImportResult playlist = ParsePlaylistImportText(text, prefix);
            if (playlist.channels.empty()) {
                qCWarning(log_network).noquote() << "Remote playlist empty url=" << RedactUrlForLog(url);
                callback(BuildError(
                    QCoreApplication::translate("SourceOpenService", "Playlist contains no playable channels")));
                return;
            }

            qCInfo(log_network).noquote()
                << "Remote playlist loaded"
                << "url=" << RedactUrlForLog(url) << "channels=" << static_cast<int>(playlist.channels.size())
                << "hasEpg=" << !playlist.epg_url.isEmpty();
            callback(ChannelListResolution{
                .channels = BuildResolvedChannels(playlist.channels, domain::SourceOrigin::kRemotePlaylist, context),
                .recent_item = recent_item,
                .playlist_epg_url = playlist.epg_url,
            });
        });
}

OpenResolution SourceOpenService::ResolveLocalPlaylist(const QString &path, const SourceOpenContext &context,
                                                       RecentOpenItem recent_item) const {
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(log_app).noquote() << "Local playlist open failed path=" << QDir::toNativeSeparators(path)
                                     << "reason=" << input.errorString();
        return BuildError(QCoreApplication::translate("SourceOpenService", "Failed to open playlist file"));
    }

    const QString text = QString::fromUtf8(input.readAll());
    const PlaylistImportResult playlist =
        ParsePlaylistImportText(text, NormalizedPlaylistPrefix(QFileInfo(path).baseName()));
    if (playlist.channels.empty()) {
        qCWarning(log_app).noquote() << "Local playlist contains no playable channels path="
                                     << QDir::toNativeSeparators(path);
        return BuildError(QCoreApplication::translate("SourceOpenService", "Playlist contains no playable channels"));
    }

    qCInfo(log_app).noquote() << "Local playlist loaded path=" << QDir::toNativeSeparators(path)
                              << "channels=" << static_cast<int>(playlist.channels.size())
                              << "hasEpg=" << !playlist.epg_url.isEmpty();
    return ChannelListResolution{
        .channels = BuildResolvedChannels(playlist.channels, domain::SourceOrigin::kLocalPlaylist, context),
        .recent_item = recent_item,
        .playlist_epg_url = playlist.epg_url,
    };
}

OpenResolution SourceOpenService::ResolveDirectMedia(const domain::Channel &channel, const SourceOpenContext &context,
                                                     domain::SourceOrigin origin,
                                                     std::optional<RecentOpenItem> recent_item) const {
    if (!channel.url.isValid() || channel.url.toString().isEmpty()) {
        qCWarning(log_app).noquote() << "Direct media rejected invalid target";
        return BuildError(
            QCoreApplication::translate("SourceOpenService", "Open request failed: invalid media target."));
    }

    const domain::SourceKind source_kind =
        channel.url.isLocalFile() ? domain::SourceKind::kLocalFile : domain::SourceKind::kDirectRemoteMedia;
    return DirectMediaResolution{
        .item =
            domain::ResolvedChannel{
                .channel = channel,
                .source = BuildDescriptor(channel, source_kind, origin, context),
            },
        .recent_item = std::move(recent_item),
    };
}

}  // namespace shatv::app
