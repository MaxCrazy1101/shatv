#include "app/m3u_playlist_parser.h"

#include <QtTest>

namespace {

using shatv::app::LooksLikeM3uPlaylistText;
using shatv::app::ParseM3uPlaylistText;
using shatv::app::ParsePlaylistImportText;

class M3uPlaylistParserTest : public QObject {
    Q_OBJECT

   private slots:
    void parses_extinf_entries_into_channels();
    void parses_playlist_level_epg_url_and_channel_epg_keys();
    void ignores_invalid_entries_and_hls_manifests();
    void local_playlist_detection_prefers_m3u_over_single_media();
    void remote_playlist_detection_uses_m3u_rules();
    void playlist_channel_detection_supports_local_remote_and_query();
};

void M3uPlaylistParserTest::parses_extinf_entries_into_channels() {
    const QString text =
        "#EXTM3U x-tvg-url=\"https://epg.example.com/t.xml\"\n"
        "#EXTINF:-1 tvg-name=\"CCTV1\" group-title=\"央视\",CCTV1综合\n"
        "https://example.com/cctv1.m3u8\n"
        "#EXTINF:-1 tvg-name=\"东方卫视\" group-title=\"卫视\",东方卫视\n"
        "https://example.com/dfws.m3u8\n";

    QVERIFY(LooksLikeM3uPlaylistText(text));

    const auto channels = ParseM3uPlaylistText(text, "playlist");

    QCOMPARE(channels.size(), 2);
    QCOMPARE(channels.at(0).name, QString("CCTV1综合"));
    QCOMPARE(channels.at(0).group, QString("央视"));
    QCOMPARE(channels.at(0).url, QUrl("https://example.com/cctv1.m3u8"));
    QCOMPARE(channels.at(1).name, QString("东方卫视"));
    QCOMPARE(channels.at(1).group, QString("卫视"));
    QCOMPARE(channels.at(1).url, QUrl("https://example.com/dfws.m3u8"));
}

void M3uPlaylistParserTest::parses_playlist_level_epg_url_and_channel_epg_keys() {
    const QString text =
        "#EXTM3U x-tvg-url=\"https://epg.example.com/tv.xml.gz\"\n"
        "#EXTINF:-1 tvg-id=\"cctv1\" tvg-name=\"CCTV1\" group-title=\"央视\",CCTV1综合\n"
        "https://example.com/cctv1.m3u8\n";

    const auto result = ParsePlaylistImportText(text, "playlist");

    QCOMPARE(result.epg_url, QString("https://epg.example.com/tv.xml.gz"));
    QCOMPARE(result.channels.size(), 1);
    QCOMPARE(result.channels.at(0).tvg_id, QString("cctv1"));
    QCOMPARE(result.channels.at(0).tvg_name, QString("CCTV1"));
    QCOMPARE(result.channels.at(0).name, QString("CCTV1综合"));
}

void M3uPlaylistParserTest::ignores_invalid_entries_and_hls_manifests() {
    const QString text =
        "#EXTM3U\n"
        "#EXTINF:-1 group-title=\"新闻\",新闻频道\n"
        "https://example.com/news.m3u8\n"
        "#EXTINF:-1 group-title=\"坏数据\",没有地址\n"
        "# just a comment\n"
        "\n";
    const QString hls_text =
        "#EXTM3U\n"
        "#EXT-X-VERSION:3\n"
        "#EXT-X-TARGETDURATION:5\n"
        "#EXTINF:5.0,\n"
        "segment000.ts\n";

    const auto channels = ParseM3uPlaylistText(text, "playlist");

    QCOMPARE(channels.size(), 1);
    QCOMPARE(channels.at(0).name, QString("新闻频道"));
    QVERIFY(!LooksLikeM3uPlaylistText(hls_text));
}

void M3uPlaylistParserTest::local_playlist_detection_prefers_m3u_over_single_media() {
    QVERIFY(shatv::app::LooksLikeLocalM3uPath("/tmp/iptv.m3u"));
    QVERIFY(!shatv::app::LooksLikeLocalM3uPath("/tmp/live.m3u8"));
    QVERIFY(!shatv::app::LooksLikeLocalM3uPath("/tmp/movie.mp4"));
}

void M3uPlaylistParserTest::remote_playlist_detection_uses_m3u_rules() {
    QVERIFY(shatv::app::LooksLikeRemoteM3uUrl(QUrl("https://example.com/iptv.m3u")));
    QVERIFY(shatv::app::LooksLikeRemoteM3uUrl(QUrl("https://example.com/iptv.m3u?userid=1&token=2")));
    QVERIFY(!shatv::app::LooksLikeRemoteM3uUrl(QUrl("https://example.com/index.m3u8")));
    QVERIFY(!shatv::app::LooksLikeRemoteM3uUrl(QUrl("https://example.com/video.mp4")));
}

void M3uPlaylistParserTest::playlist_channel_detection_supports_local_remote_and_query() {
    shatv::domain::Channel local_channel;
    local_channel.url = QUrl::fromLocalFile("/tmp/iptv.m3u");

    shatv::domain::Channel remote_channel;
    remote_channel.url = QUrl("https://example.com/iptv.m3u?userid=1&token=2");

    shatv::domain::Channel hls_channel;
    hls_channel.url = QUrl("https://example.com/index.m3u8");

    QVERIFY(shatv::app::LooksLikePlaylistChannel(local_channel));
    QVERIFY(shatv::app::LooksLikePlaylistChannel(remote_channel));
    QVERIFY(!shatv::app::LooksLikePlaylistChannel(hls_channel));
}

}  // namespace

QTEST_GUILESS_MAIN(M3uPlaylistParserTest)

#include "m3u_playlist_parser_test.moc"
