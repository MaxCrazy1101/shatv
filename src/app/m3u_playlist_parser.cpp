#include "app/m3u_playlist_parser.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

namespace shatv::app {

namespace {

QString ExtractAttribute(const QString &line, const QString &key) {
    const QRegularExpression pattern(QString("%1=\"([^\"]*)\"").arg(QRegularExpression::escape(key)));
    const QRegularExpressionMatch match = pattern.match(line);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString ChannelNameFromUrl(const QUrl &url) {
    const QString file_name = QFileInfo(url.path()).fileName();
    if (!file_name.isEmpty()) {
        return file_name;
    }
    if (!url.host().isEmpty()) {
        return url.host();
    }
    return url.toString();
}

}  // namespace

bool LooksLikeLocalM3uPath(const QString &path) {
    return QFileInfo(path).suffix().compare("m3u", Qt::CaseInsensitive) == 0;
}

bool LooksLikeRemoteM3uUrl(const QUrl &url) {
    if (!url.isValid() || url.isLocalFile()) {
        return false;
    }
    return QFileInfo(url.path()).suffix().compare("m3u", Qt::CaseInsensitive) == 0;
}

bool LooksLikePlaylistChannel(const domain::Channel &channel) {
    if (!channel.url.isValid()) {
        return false;
    }
    if (channel.url.isLocalFile()) {
        return LooksLikeLocalM3uPath(channel.url.toLocalFile());
    }
    return LooksLikeRemoteM3uUrl(channel.url);
}

bool LooksLikeM3uPlaylistText(const QString &text) {
    return text.contains("#EXTINF:") && !text.contains("#EXT-X-TARGETDURATION:");
}

std::vector<domain::Channel> ParseM3uPlaylistText(const QString &text, const QString &id_prefix) {
    return ParsePlaylistImportText(text, id_prefix).channels;
}

PlaylistImportResult ParsePlaylistImportText(const QString &text, const QString &id_prefix) {
    PlaylistImportResult result;
    const QStringList lines = text.split('\n');

    QString pending_name;
    QString pending_group;
    QString pending_tvg_id;
    QString pending_tvg_name;
    bool has_pending_item = false;

    for (const QString &raw_line : lines) {
        const QString line = raw_line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        // #EXTM3U 头里可能带 x-tvg-url，这属于播放列表级元数据，不复制到单个频道上。
        if (line.startsWith("#EXTM3U")) {
            result.epg_url = ExtractAttribute(line, "x-tvg-url");
            continue;
        }

        if (line.startsWith("#EXTINF:")) {
            const int comma_index = line.indexOf(',');
            pending_name = comma_index >= 0 ? line.mid(comma_index + 1).trimmed() : QString();
            pending_tvg_id = ExtractAttribute(line, "tvg-id");
            pending_tvg_name = ExtractAttribute(line, "tvg-name");
            if (pending_name.isEmpty()) {
                pending_name = pending_tvg_name;
            }
            pending_group = ExtractAttribute(line, "group-title");
            has_pending_item = true;
            continue;
        }

        if (line.startsWith('#')) {
            continue;
        }

        if (!has_pending_item) {
            continue;
        }

        const QUrl url = QUrl::fromUserInput(line);
        if (!url.isValid() || url.toString().isEmpty()) {
            has_pending_item = false;
            pending_name.clear();
            pending_group.clear();
            pending_tvg_id.clear();
            pending_tvg_name.clear();
            continue;
        }

        // 第一阶段先产出可直接播放的频道数据，并顺手保留 EPG 绑定键供后续 XMLTV 匹配层使用。
        const QString channel_name = pending_name.isEmpty() ? ChannelNameFromUrl(url) : pending_name;
        const QString channel_id = QString("%1-%2-%3").arg(id_prefix).arg(result.channels.size()).arg(channel_name);

        result.channels.push_back(domain::Channel{
            .id = channel_id,
            .name = channel_name,
            .url = url,
            .group = pending_group,
            .tvg_id = pending_tvg_id,
            .tvg_name = pending_tvg_name,
        });

        has_pending_item = false;
        pending_name.clear();
        pending_group.clear();
        pending_tvg_id.clear();
        pending_tvg_name.clear();
    }

    return result;
}

}  // namespace shatv::app
