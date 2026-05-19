#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

namespace shatv::app {

struct XmltvChannel {
    QString id;
    QStringList display_names;
};

struct XmltvProgramme {
    QString channel_id;
    QDateTime start_at;
    QDateTime stop_at;
    QString title;
};

struct XmltvParseResult {
    std::vector<XmltvChannel> channels;
    std::vector<XmltvProgramme> programmes;
};

std::optional<XmltvParseResult> ParseXmltvDocument(const QString &xml, QString *error_message = nullptr);

}  // namespace shatv::app
