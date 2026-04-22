#include "app/xmltv_epg_parser.h"

#include <QTimeZone>
#include <QXmlStreamReader>

namespace shatv::app {

namespace {

QDateTime ParseXmltvTimestamp(const QString &raw) {
    const QString trimmed = raw.trimmed();
    if (trimmed.size() != 20 || trimmed.at(14) != ' ') {
        return {};
    }

    bool ok = false;
    const int year = trimmed.mid(0, 4).toInt(&ok);
    if (!ok) {
        return {};
    }
    const int month = trimmed.mid(4, 2).toInt(&ok);
    if (!ok) {
        return {};
    }
    const int day = trimmed.mid(6, 2).toInt(&ok);
    if (!ok) {
        return {};
    }
    const int hour = trimmed.mid(8, 2).toInt(&ok);
    if (!ok) {
        return {};
    }
    const int minute = trimmed.mid(10, 2).toInt(&ok);
    if (!ok) {
        return {};
    }
    const int second = trimmed.mid(12, 2).toInt(&ok);
    if (!ok) {
        return {};
    }

    const QChar sign = trimmed.at(15);
    if (sign != '+' && sign != '-') {
        return {};
    }

    const int offset_hours = trimmed.mid(16, 2).toInt(&ok);
    if (!ok) {
        return {};
    }
    const int offset_minutes = trimmed.mid(18, 2).toInt(&ok);
    if (!ok) {
        return {};
    }

    const QDate date(year, month, day);
    const QTime time(hour, minute, second);
    if (!date.isValid() || !time.isValid()) {
        return {};
    }

    const int offset_seconds = (offset_hours * 3600 + offset_minutes * 60) * (sign == '-' ? -1 : 1);
    const QTimeZone time_zone = QTimeZone::fromSecondsAheadOfUtc(offset_seconds);
    if (!time_zone.isValid()) {
        return {};
    }

    return QDateTime(date, time, time_zone);
}

XmltvChannel ParseChannelElement(QXmlStreamReader &reader) {
    XmltvChannel channel;
    channel.id = reader.attributes().value("id").toString().trimmed();

    while (reader.readNextStartElement()) {
        if (reader.name() == u"display-name") {
            const QString display_name = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            if (!display_name.isEmpty()) {
                channel.display_names.push_back(display_name);
            }
            continue;
        }

        reader.skipCurrentElement();
    }

    return channel;
}

std::optional<XmltvProgramme> ParseProgrammeElement(QXmlStreamReader &reader) {
    XmltvProgramme programme;
    programme.channel_id = reader.attributes().value("channel").toString().trimmed();
    programme.start_at = ParseXmltvTimestamp(reader.attributes().value("start").toString());
    programme.stop_at = ParseXmltvTimestamp(reader.attributes().value("stop").toString());

    while (reader.readNextStartElement()) {
        if (reader.name() == u"title") {
            if (programme.title.isEmpty()) {
                programme.title = reader.readElementText(QXmlStreamReader::SkipChildElements).trimmed();
            } else {
                reader.skipCurrentElement();
            }
            continue;
        }

        reader.skipCurrentElement();
    }

    // 第一阶段只保留后续 now/next 查询真正需要的最小节目字段，坏时间戳直接丢弃。
    if (programme.channel_id.isEmpty() || !programme.start_at.isValid() || !programme.stop_at.isValid()) {
        return std::nullopt;
    }

    return programme;
}

}  // namespace

std::optional<XmltvParseResult> ParseXmltvDocument(const QString &xml, QString *error_message) {
    if (error_message != nullptr) {
        error_message->clear();
    }

    QXmlStreamReader reader(xml);
    XmltvParseResult result;

    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }

        if (reader.name() == u"channel") {
            result.channels.push_back(ParseChannelElement(reader));
            continue;
        }

        if (reader.name() == u"programme") {
            if (const std::optional<XmltvProgramme> programme = ParseProgrammeElement(reader); programme.has_value()) {
                result.programmes.push_back(*programme);
            }
            continue;
        }
    }

    if (reader.hasError()) {
        if (error_message != nullptr) {
            *error_message = reader.errorString();
        }
        return std::nullopt;
    }

    return result;
}

}  // namespace shatv::app
