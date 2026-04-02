#include <QtTest>

#include "app/m3u_playlist_parser.h"

namespace {

using shatv::app::LooksLikeM3uPlaylistText;
using shatv::app::ParseM3uPlaylistText;

class M3uPlaylistParserTest : public QObject {
    Q_OBJECT

   private slots:
    void parses_extinf_entries_into_channels();
    void ignores_invalid_entries_and_hls_manifests();
    void local_playlist_detection_prefers_m3u_over_single_media();
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

}  // namespace

QTEST_GUILESS_MAIN(M3uPlaylistParserTest)

#include "m3u_playlist_parser_test.moc"
