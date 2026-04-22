#include <QtTest>

#include <QTimeZone>

#include "app/xmltv_epg_parser.h"

namespace {

using shatv::app::ParseXmltvDocument;

class XmltvEpgParserTest : public QObject {
    Q_OBJECT

   private slots:
    void parses_channels_and_programmes();
    void skips_programmes_with_invalid_timestamps();
    void rejects_malformed_xml();
};

void XmltvEpgParserTest::parses_channels_and_programmes() {
    const QString xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<tv>\n"
        "  <channel id=\"cctv1\">\n"
        "    <display-name>CCTV1</display-name>\n"
        "    <display-name>CCTV-1</display-name>\n"
        "  </channel>\n"
        "  <programme channel=\"cctv1\" start=\"20260422080000 +0800\" stop=\"20260422090000 +0800\">\n"
        "    <title>朝闻天下</title>\n"
        "  </programme>\n"
        "</tv>\n";

    QString error_message;
    const auto parsed = ParseXmltvDocument(xml, &error_message);

    QVERIFY2(parsed.has_value(), qPrintable(error_message));
    QVERIFY(error_message.isEmpty());
    QCOMPARE(parsed->channels.size(), 1);
    QCOMPARE(parsed->channels.at(0).id, QString("cctv1"));
    QCOMPARE(parsed->channels.at(0).display_names.size(), 2);
    QCOMPARE(parsed->channels.at(0).display_names.at(0), QString("CCTV1"));
    QCOMPARE(parsed->channels.at(0).display_names.at(1), QString("CCTV-1"));
    QCOMPARE(parsed->programmes.size(), 1);
    QCOMPARE(parsed->programmes.at(0).channel_id, QString("cctv1"));
    QCOMPARE(parsed->programmes.at(0).title, QString("朝闻天下"));
    QCOMPARE(parsed->programmes.at(0).start_at.toUTC(),
             QDateTime(QDate(2026, 4, 22), QTime(0, 0), QTimeZone(QTimeZone::UTC)));
    QCOMPARE(parsed->programmes.at(0).stop_at.toUTC(),
             QDateTime(QDate(2026, 4, 22), QTime(1, 0), QTimeZone(QTimeZone::UTC)));
}

void XmltvEpgParserTest::skips_programmes_with_invalid_timestamps() {
    const QString xml =
        "<tv>\n"
        "  <channel id=\"cctv1\">\n"
        "    <display-name>CCTV1</display-name>\n"
        "  </channel>\n"
        "  <programme channel=\"cctv1\" start=\"bad-time\" stop=\"20260422090000 +0800\">\n"
        "    <title>无效节目</title>\n"
        "  </programme>\n"
        "  <programme channel=\"cctv1\" start=\"20260422090000 +0800\" stop=\"20260422100000 +0800\">\n"
        "    <title>新闻联播</title>\n"
        "  </programme>\n"
        "</tv>\n";

    QString error_message;
    const auto parsed = ParseXmltvDocument(xml, &error_message);

    QVERIFY2(parsed.has_value(), qPrintable(error_message));
    QCOMPARE(parsed->programmes.size(), 1);
    QCOMPARE(parsed->programmes.at(0).title, QString("新闻联播"));
}

void XmltvEpgParserTest::rejects_malformed_xml() {
    QString error_message;
    const auto parsed = ParseXmltvDocument("<tv><channel id=\"cctv1\"></tv>", &error_message);

    QVERIFY(!parsed.has_value());
    QVERIFY(!error_message.isEmpty());
}

}  // namespace

QTEST_GUILESS_MAIN(XmltvEpgParserTest)

#include "xmltv_epg_parser_test.moc"
