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

bool LooksLikeM3uPlaylistText(const QString &text) {
    return text.contains("#EXTINF:") && !text.contains("#EXT-X-TARGETDURATION:");
}

std::vector<domain::Channel> ParseM3uPlaylistText(const QString &text, const QString &id_prefix) {
    std::vector<domain::Channel> channels;
    const QStringList lines = text.split('\n');

    QString pending_name;
    QString pending_group;
    bool has_pending_item = false;

    for (const QString &raw_line : lines) {
        const QString line = raw_line.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        if (line.startsWith("#EXTINF:")) {
            const int comma_index = line.indexOf(',');
            pending_name = comma_index >= 0 ? line.mid(comma_index + 1).trimmed() : QString();
            if (pending_name.isEmpty()) {
                pending_name = ExtractAttribute(line, "tvg-name");
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
            continue;
        }

        // 解析器只产出 UI 立即可用的最小频道数据，不引入额外模型字段。
        const QString channel_name = pending_name.isEmpty() ? ChannelNameFromUrl(url) : pending_name;
        const QString channel_id = QString("%1-%2-%3").arg(id_prefix).arg(channels.size()).arg(channel_name);

        channels.push_back(domain::Channel{
            .id = channel_id,
            .name = channel_name,
            .url = url,
            .group = pending_group,
        });

        has_pending_item = false;
        pending_name.clear();
        pending_group.clear();
    }

    return channels;
}

}  // namespace shatv::app
