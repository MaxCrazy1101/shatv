#include <QtTest>

#include <QTimeZone>

#include "app/epg_service.h"

namespace {

using shatv::app::ChannelEpgNowNext;
using shatv::app::EpgService;

QDateTime UtcDateTime(int year, int month, int day, int hour, int minute) {
    return QDateTime(QDate(year, month, day), QTime(hour, minute), QTimeZone(QTimeZone::UTC));
}

class EpgServiceTest : public QObject {
    Q_OBJECT

   private slots:
    void resolve_source_url_prefers_settings_over_playlist();
    void resolve_source_url_falls_back_to_playlist_url();
    void lookup_now_next_prefers_tvg_id_matches();
    void lookup_now_next_falls_back_to_tvg_name_and_channel_name();
    void lookup_now_next_returns_empty_when_no_channel_matches();
};

void EpgServiceTest::resolve_source_url_prefers_settings_over_playlist() {
    QCOMPARE(EpgService::ResolveSourceUrl(" https://settings.example.com/epg.xml.gz ",
                                          "https://playlist.example.com/epg.xml"),
             QString("https://settings.example.com/epg.xml.gz"));
}

void EpgServiceTest::resolve_source_url_falls_back_to_playlist_url() {
    QCOMPARE(EpgService::ResolveSourceUrl("   ", " https://playlist.example.com/epg.xml "),
             QString("https://playlist.example.com/epg.xml"));
}

void EpgServiceTest::lookup_now_next_prefers_tvg_id_matches() {
    const QString xml =
        "<tv>\n"
        "  <channel id=\"cctv1\">\n"
        "    <display-name>CCTV1</display-name>\n"
        "  </channel>\n"
        "  <channel id=\"other\">\n"
        "    <display-name>CCTV1综合</display-name>\n"
        "  </channel>\n"
        "  <programme channel=\"cctv1\" start=\"20260422080000 +0800\" stop=\"20260422090000 +0800\">\n"
        "    <title>朝闻天下</title>\n"
        "  </programme>\n"
        "  <programme channel=\"cctv1\" start=\"20260422090000 +0800\" stop=\"20260422100000 +0800\">\n"
        "    <title>新闻30分</title>\n"
        "  </programme>\n"
        "  <programme channel=\"other\" start=\"20260422080000 +0800\" stop=\"20260422090000 +0800\">\n"
        "    <title>错误匹配节目</title>\n"
        "  </programme>\n"
        "</tv>\n";

    EpgService service;
    QString error_message;
    QVERIFY2(service.LoadXmltv(xml, &error_message), qPrintable(error_message));

    const shatv::domain::Channel channel{
        .id = "channel-1",
        .name = "CCTV1综合",
        .url = QUrl("https://example.com/cctv1.m3u8"),
        .group = "央视",
        .tvg_id = "cctv1",
        .tvg_name = "CCTV1",
    };

    const ChannelEpgNowNext now_next = service.LookupNowNext(channel, UtcDateTime(2026, 4, 22, 0, 30));

    QVERIFY(now_next.current.has_value());
    QVERIFY(now_next.next.has_value());
    QCOMPARE(now_next.current->title, QString("朝闻天下"));
    QCOMPARE(now_next.next->title, QString("新闻30分"));
}

void EpgServiceTest::lookup_now_next_falls_back_to_tvg_name_and_channel_name() {
    const QString xml =
        "<tv>\n"
        "  <channel id=\"dragontv\">\n"
        "    <display-name>东方卫视</display-name>\n"
        "  </channel>\n"
        "  <programme channel=\"dragontv\" start=\"20260422080000 +0800\" stop=\"20260422090000 +0800\">\n"
        "    <title>看东方</title>\n"
        "  </programme>\n"
        "  <programme channel=\"dragontv\" start=\"20260422090000 +0800\" stop=\"20260422100000 +0800\">\n"
        "    <title>新闻坊</title>\n"
        "  </programme>\n"
        "</tv>\n";

    EpgService service;
    QString error_message;
    QVERIFY2(service.LoadXmltv(xml, &error_message), qPrintable(error_message));

    const shatv::domain::Channel tvg_name_channel{
        .id = "channel-1",
        .name = "上海东方卫视",
        .url = QUrl("https://example.com/dfws.m3u8"),
        .group = "卫视",
        .tvg_id = "",
        .tvg_name = "  东方卫视  ",
    };
    const ChannelEpgNowNext from_tvg_name = service.LookupNowNext(tvg_name_channel, UtcDateTime(2026, 4, 22, 0, 30));
    QVERIFY(from_tvg_name.current.has_value());
    QCOMPARE(from_tvg_name.current->title, QString("看东方"));

    const shatv::domain::Channel channel_name_only{
        .id = "channel-2",
        .name = "东方卫视",
        .url = QUrl("https://example.com/dfws-hd.m3u8"),
        .group = "卫视",
        .tvg_id = "",
        .tvg_name = "",
    };
    const ChannelEpgNowNext from_channel_name =
        service.LookupNowNext(channel_name_only, UtcDateTime(2026, 4, 22, 0, 30));
    QVERIFY(from_channel_name.current.has_value());
    QCOMPARE(from_channel_name.current->title, QString("看东方"));
    QVERIFY(from_channel_name.next.has_value());
    QCOMPARE(from_channel_name.next->title, QString("新闻坊"));
}

void EpgServiceTest::lookup_now_next_returns_empty_when_no_channel_matches() {
    const QString xml =
        "<tv>\n"
        "  <channel id=\"cctv1\">\n"
        "    <display-name>CCTV1</display-name>\n"
        "  </channel>\n"
        "</tv>\n";

    EpgService service;
    QString error_message;
    QVERIFY2(service.LoadXmltv(xml, &error_message), qPrintable(error_message));

    const shatv::domain::Channel channel{
        .id = "channel-1",
        .name = "不存在的频道",
        .url = QUrl("https://example.com/missing.m3u8"),
        .group = "其他",
        .tvg_id = "",
        .tvg_name = "",
    };

    const ChannelEpgNowNext now_next = service.LookupNowNext(channel, UtcDateTime(2026, 4, 22, 0, 30));

    QVERIFY(!now_next.current.has_value());
    QVERIFY(!now_next.next.has_value());
}

}  // namespace

QTEST_GUILESS_MAIN(EpgServiceTest)

#include "epg_service_test.moc"
