#include <QtTest>

#include "app/launch_options.h"

namespace {

using shatv::app::BuildStartupChannel;
using shatv::app::LaunchOptions;
using shatv::app::ParseLaunchOptions;

class LaunchOptionsTest : public QObject {
    Q_OBJECT

   private slots:
    void parse_open_media_argument();
    void build_startup_channel_from_open_url_argument();
    void build_startup_channel_from_local_media();
    void build_startup_channel_from_http_url();
    void build_startup_channel_prefers_open_url_over_open_media();
};

void LaunchOptionsTest::parse_open_media_argument() {
    const LaunchOptions options =
        ParseLaunchOptions({"shatv", "--open-media", "./docs/file_example_MP4_1920_18MG.mp4"});

    QCOMPARE(options.smoke_test, false);
    QCOMPARE(options.mpv_smoke, false);
    QCOMPARE(options.open_media_argument, QString("./docs/file_example_MP4_1920_18MG.mp4"));
}

void LaunchOptionsTest::build_startup_channel_from_open_url_argument() {
    const LaunchOptions options =
        ParseLaunchOptions({"shatv", "--open-url", "http://127.0.0.1:8080/index.m3u8"});

    const auto channel = BuildStartupChannel(options, QString(), "/home/alex/code/shatv");

    QVERIFY(channel.has_value());
    QCOMPARE(channel->id, QString("open-url"));
    QCOMPARE(channel->name, QString("index.m3u8"));
    QCOMPARE(channel->url, QUrl("http://127.0.0.1:8080/index.m3u8"));
}

void LaunchOptionsTest::build_startup_channel_from_local_media() {
    LaunchOptions options;
    options.open_media_argument = "./docs/file_example_MP4_1920_18MG.mp4";

    const auto channel = BuildStartupChannel(options, QString(), "/home/alex/code/shatv");

    QVERIFY(channel.has_value());
    QCOMPARE(channel->id, QString("open-media"));
    QCOMPARE(channel->name, QString("file_example_MP4_1920_18MG.mp4"));
    QVERIFY(channel->url.isLocalFile());
    QCOMPARE(channel->url.toLocalFile(), QString("/home/alex/code/shatv/docs/file_example_MP4_1920_18MG.mp4"));
}

void LaunchOptionsTest::build_startup_channel_from_http_url() {
    LaunchOptions options;
    options.open_media_argument = "http://127.0.0.1:8080/live.m3u8";

    const auto channel = BuildStartupChannel(options, QString(), "/home/alex/code/shatv");

    QVERIFY(channel.has_value());
    QCOMPARE(channel->id, QString("open-media"));
    QCOMPARE(channel->name, QString("live.m3u8"));
    QCOMPARE(channel->url, QUrl("http://127.0.0.1:8080/live.m3u8"));
}

void LaunchOptionsTest::build_startup_channel_prefers_open_url_over_open_media() {
    const LaunchOptions options = ParseLaunchOptions(
        {"shatv", "--open-media", "./docs/file_example_MP4_1920_18MG.mp4", "--open-url",
         "http://127.0.0.1:8080/index.m3u8"});

    const auto channel = BuildStartupChannel(options, QString(), "/home/alex/code/shatv");

    QVERIFY(channel.has_value());
    QCOMPARE(channel->id, QString("open-url"));
    QCOMPARE(channel->name, QString("index.m3u8"));
    QCOMPARE(channel->url, QUrl("http://127.0.0.1:8080/index.m3u8"));
}

}  // namespace

QTEST_GUILESS_MAIN(LaunchOptionsTest)

#include "launch_options_test.moc"
