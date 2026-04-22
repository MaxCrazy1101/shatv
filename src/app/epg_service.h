#pragma once

#include <optional>
#include <vector>

#include <QDateTime>
#include <QHash>
#include <QString>

#include "app/xmltv_epg_parser.h"
#include "domain/channel.h"

namespace shatv::app {

struct ChannelEpgNowNext {
    std::optional<XmltvProgramme> current;
    std::optional<XmltvProgramme> next;
};

class EpgService final {
   public:
    static QString ResolveSourceUrl(const QString &settings_epg_url, const QString &playlist_epg_url);

    bool LoadXmltv(const QString &xml, QString *error_message = nullptr);
    ChannelEpgNowNext LookupNowNext(const domain::Channel &channel, const QDateTime &now) const;

   private:
    QString ResolveChannelId(const domain::Channel &channel) const;

    QHash<QString, QString> channel_id_by_normalized_name_;
    QHash<QString, std::vector<XmltvProgramme>> programmes_by_channel_id_;
};

}  // namespace shatv::app
