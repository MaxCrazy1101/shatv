#include "app/epg_service.h"

#include <algorithm>

#include <QHash>

namespace shatv::app {

namespace {

QString NormalizeLookupKey(const QString &value) {
    return value.simplified().toLower();
}

}  // namespace

QString EpgService::ResolveSourceUrl(const QString &settings_epg_url, const QString &playlist_epg_url) {
    const QString normalized_settings_url = settings_epg_url.trimmed();
    if (!normalized_settings_url.isEmpty()) {
        return normalized_settings_url;
    }

    return playlist_epg_url.trimmed();
}

bool EpgService::LoadXmltv(const QString &xml, QString *error_message) {
    const std::optional<XmltvParseResult> parsed = ParseXmltvDocument(xml, error_message);
    if (!parsed.has_value()) {
        return false;
    }

    channel_id_by_normalized_name_.clear();
    programmes_by_channel_id_.clear();

    for (const XmltvChannel &channel : parsed->channels) {
        for (const QString &display_name : channel.display_names) {
            const QString normalized_name = NormalizeLookupKey(display_name);
            if (normalized_name.isEmpty() || channel_id_by_normalized_name_.contains(normalized_name)) {
                continue;
            }
            channel_id_by_normalized_name_.insert(normalized_name, channel.id);
        }
    }

    for (const XmltvProgramme &programme : parsed->programmes) {
        programmes_by_channel_id_[programme.channel_id].push_back(programme);
    }

    for (auto it = programmes_by_channel_id_.begin(); it != programmes_by_channel_id_.end(); ++it) {
        std::sort(it->begin(), it->end(), [](const XmltvProgramme &lhs, const XmltvProgramme &rhs) {
            if (lhs.start_at == rhs.start_at) {
                return lhs.stop_at < rhs.stop_at;
            }
            return lhs.start_at < rhs.start_at;
        });
    }

    return true;
}

ChannelEpgNowNext EpgService::LookupNowNext(const domain::Channel &channel, const QDateTime &now) const {
    const QString channel_id = ResolveChannelId(channel);
    if (channel_id.isEmpty()) {
        return {};
    }

    const auto programmes_it = programmes_by_channel_id_.find(channel_id);
    if (programmes_it == programmes_by_channel_id_.end()) {
        return {};
    }

    ChannelEpgNowNext now_next;
    const std::vector<XmltvProgramme> &programmes = programmes_it.value();

    for (std::size_t index = 0; index < programmes.size(); ++index) {
        const XmltvProgramme &programme = programmes[index];
        if (programme.start_at <= now && now < programme.stop_at) {
            now_next.current = programme;
            if (index + 1 < programmes.size()) {
                now_next.next = programmes[index + 1];
            }
            return now_next;
        }

        if (!now_next.next.has_value() && now < programme.start_at) {
            now_next.next = programme;
        }
    }

    return now_next;
}

QString EpgService::ResolveChannelId(const domain::Channel &channel) const {
    const QString tvg_id = channel.tvg_id.trimmed();
    if (!tvg_id.isEmpty() && programmes_by_channel_id_.contains(tvg_id)) {
        return tvg_id;
    }

    // 匹配优先级固定为 tvg-id -> tvg-name -> channel.name，第一阶段只做确定性的精确/规范化匹配。
    const QString normalized_tvg_name = NormalizeLookupKey(channel.tvg_name);
    if (!normalized_tvg_name.isEmpty()) {
        const auto tvg_name_it = channel_id_by_normalized_name_.find(normalized_tvg_name);
        if (tvg_name_it != channel_id_by_normalized_name_.end()) {
            return tvg_name_it.value();
        }
    }

    const QString normalized_channel_name = NormalizeLookupKey(channel.name);
    if (!normalized_channel_name.isEmpty()) {
        const auto channel_name_it = channel_id_by_normalized_name_.find(normalized_channel_name);
        if (channel_name_it != channel_id_by_normalized_name_.end()) {
            return channel_name_it.value();
        }
    }

    return {};
}

}  // namespace shatv::app
