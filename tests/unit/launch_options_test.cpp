#include <QtTest>
#include <QFile>
#include <QTemporaryDir>

#include "app/app_settings.h"
#include "app/launch_options.h"

namespace {

using shatv::app::BuildStartupChannel;
using shatv::app::AppSettings;
using shatv::app::IsRemotePlaybackUrl;
using shatv::app::LooksLikeRemoteMediaDirectoryUrl;
using shatv::app::LaunchOptions;
using shatv::app::OpenRequestKind;
using shatv::app::ParseLaunchOptions;
using shatv::app::RecentOpenItem;

template <typename Enum>
int EnumValue(Enum value) {
    return static_cast<int>(value);
}

class LaunchOptionsTest : public QObject {
    Q_OBJECT

   private slots:
    void parse_open_media_argument();
    void parse_ffmpeg_audio_smoke_flag();
    void parse_ffmpeg_smoke_flag();
    void build_startup_channel_from_open_url_argument();
    void build_startup_channel_from_local_media();
    void build_startup_channel_from_http_url();
    void build_startup_channel_prefers_open_url_over_open_media();
    void remote_playback_url_detection();
    void remote_root_url_detection_for_open_link_validation();
    void load_missing_settings_defaults_to_empty_user_agent();
    void load_missing_settings_defaults_to_empty_epg_url();
    void load_missing_settings_defaults_to_osd_auto_hide_three_seconds();
    void save_and_load_user_agent_round_trip();
    void save_and_load_epg_url_round_trip();
    void save_and_load_osd_auto_hide_seconds_round_trip();
    void save_and_load_recent_items_with_dedup_and_limit();
    void load_recent_items_deduplicates_existing_config_entries();
    void load_invalid_osd_auto_hide_seconds_falls_back_to_default();
    void save_settings_preserves_existing_comments_and_fields();
};

void LaunchOptionsTest::parse_open_media_argument() {
    const LaunchOptions options =
        ParseLaunchOptions({"shatv", "--open-media", "./docs/file_example_MP4_1920_18MG.mp4"});

    QCOMPARE(options.ffmpeg_audio_smoke, false);
    QCOMPARE(options.open_media_argument, QString("./docs/file_example_MP4_1920_18MG.mp4"));
}

void LaunchOptionsTest::parse_ffmpeg_audio_smoke_flag() {
    const LaunchOptions options = ParseLaunchOptions({"shatv", "--ffmpeg-audio-smoke"});

    QCOMPARE(options.ffmpeg_audio_smoke, true);
    QCOMPARE(options.ffmpeg_smoke, false);
}

void LaunchOptionsTest::parse_ffmpeg_smoke_flag() {
    const LaunchOptions options = ParseLaunchOptions({"shatv", "--ffmpeg-smoke"});

    QCOMPARE(options.ffmpeg_smoke, true);
    QCOMPARE(options.ffmpeg_audio_smoke, false);
}

void LaunchOptionsTest::build_startup_channel_from_open_url_argument() {
    const LaunchOptions options =
        ParseLaunchOptions({"shatv", "--open-url", "http://127.0.0.1:8080/index.m3u8"});

    const auto channel = BuildStartupChannel(options, "/home/alex/code/shatv");

    QVERIFY(channel.has_value());
    QCOMPARE(channel->id, QString("open-url"));
    QCOMPARE(channel->name, QString("index.m3u8"));
    QCOMPARE(channel->url, QUrl("http://127.0.0.1:8080/index.m3u8"));
}

void LaunchOptionsTest::build_startup_channel_from_local_media() {
    LaunchOptions options;
    options.open_media_argument = "./docs/file_example_MP4_1920_18MG.mp4";

    const auto channel = BuildStartupChannel(options, "/home/alex/code/shatv");

    QVERIFY(channel.has_value());
    QCOMPARE(channel->id, QString("open-media"));
    QCOMPARE(channel->name, QString("file_example_MP4_1920_18MG.mp4"));
    QVERIFY(channel->url.isLocalFile());
    QCOMPARE(channel->url.toLocalFile(), QString("/home/alex/code/shatv/docs/file_example_MP4_1920_18MG.mp4"));
}

void LaunchOptionsTest::build_startup_channel_from_http_url() {
    LaunchOptions options;
    options.open_media_argument = "http://127.0.0.1:8080/live.m3u8";

    const auto channel = BuildStartupChannel(options, "/home/alex/code/shatv");

    QVERIFY(channel.has_value());
    QCOMPARE(channel->id, QString("open-media"));
    QCOMPARE(channel->name, QString("live.m3u8"));
    QCOMPARE(channel->url, QUrl("http://127.0.0.1:8080/live.m3u8"));
}

void LaunchOptionsTest::build_startup_channel_prefers_open_url_over_open_media() {
    const LaunchOptions options = ParseLaunchOptions(
        {"shatv", "--open-media", "./docs/file_example_MP4_1920_18MG.mp4", "--open-url",
         "http://127.0.0.1:8080/index.m3u8"});

    const auto channel = BuildStartupChannel(options, "/home/alex/code/shatv");

    QVERIFY(channel.has_value());
    QCOMPARE(channel->id, QString("open-url"));
    QCOMPARE(channel->name, QString("index.m3u8"));
    QCOMPARE(channel->url, QUrl("http://127.0.0.1:8080/index.m3u8"));
}

void LaunchOptionsTest::remote_playback_url_detection() {
    QVERIFY(IsRemotePlaybackUrl(QUrl("http://127.0.0.1:8080/index.m3u8")));
    QVERIFY(IsRemotePlaybackUrl(QUrl("https://example.com/live.m3u8")));
    QVERIFY(!IsRemotePlaybackUrl(QUrl::fromLocalFile("/home/alex/code/shatv/docs/video.mp4")));
}

void LaunchOptionsTest::remote_root_url_detection_for_open_link_validation() {
    QVERIFY(LooksLikeRemoteMediaDirectoryUrl(QUrl("http://127.0.0.1:8080")));
    QVERIFY(LooksLikeRemoteMediaDirectoryUrl(QUrl("https://example.com/")));
    QVERIFY(!LooksLikeRemoteMediaDirectoryUrl(QUrl("http://127.0.0.1:8080/index.m3u8")));
    QVERIFY(!LooksLikeRemoteMediaDirectoryUrl(QUrl("http://127.0.0.1:8080/?play=1")));
}

void LaunchOptionsTest::load_missing_settings_defaults_to_empty_user_agent() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("settings/config.toml");
    AppSettings settings(config_path);

    QVERIFY(settings.Load());
    QCOMPARE(settings.UserAgent(), QString());
}

void LaunchOptionsTest::load_missing_settings_defaults_to_empty_epg_url() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("settings/config.toml");
    AppSettings settings(config_path);

    QVERIFY(settings.Load());
    QCOMPARE(settings.EpgUrl(), QString());
}

void LaunchOptionsTest::load_missing_settings_defaults_to_osd_auto_hide_three_seconds() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("settings/config.toml");
    AppSettings settings(config_path);

    QVERIFY(settings.Load());
    QCOMPARE(settings.OsdAutoHideSeconds(), 3);
}

void LaunchOptionsTest::save_and_load_user_agent_round_trip() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("settings/config.toml");

    AppSettings settings(config_path);
    settings.SetUserAgent("ShaTV QA Agent/1.0");
    QVERIFY(settings.Save());

    AppSettings reloaded(config_path);
    QVERIFY(reloaded.Load());
    QCOMPARE(reloaded.UserAgent(), QString("ShaTV QA Agent/1.0"));
}

void LaunchOptionsTest::save_and_load_epg_url_round_trip() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("settings/config.toml");

    AppSettings settings(config_path);
    settings.SetEpgUrl("https://epg.example.com/guide.xml.gz");
    QVERIFY(settings.Save());

    AppSettings reloaded(config_path);
    QVERIFY(reloaded.Load());
    QCOMPARE(reloaded.EpgUrl(), QString("https://epg.example.com/guide.xml.gz"));
}

void LaunchOptionsTest::save_and_load_osd_auto_hide_seconds_round_trip() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("settings/config.toml");
    AppSettings settings(config_path);
    settings.SetOsdAutoHideSeconds(5);
    QVERIFY(settings.Save());

    AppSettings reloaded(config_path);
    QVERIFY(reloaded.Load());
    QCOMPARE(reloaded.OsdAutoHideSeconds(), 5);
}

void LaunchOptionsTest::save_and_load_recent_items_with_dedup_and_limit() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("settings/config.toml");

    AppSettings settings(config_path);
    settings.RememberRecentItem(RecentOpenItem{
        .request_kind = OpenRequestKind::kUrlText,
        .target = "https://example.com/live-1.m3u8",
        .label = "live-1.m3u8",
    });
    settings.RememberRecentItem(RecentOpenItem{
        .request_kind = OpenRequestKind::kFilePath,
        .target = "/tmp/playlist-1.m3u",
        .label = "playlist-1.m3u",
    });
    settings.RememberRecentItem(RecentOpenItem{
        .request_kind = OpenRequestKind::kUrlText,
        .target = "https://example.com/live-2.m3u8",
        .label = "live-2.m3u8",
    });
    settings.RememberRecentItem(RecentOpenItem{
        .request_kind = OpenRequestKind::kFilePath,
        .target = "/tmp/playlist-2.m3u",
        .label = "playlist-2.m3u",
    });
    settings.RememberRecentItem(RecentOpenItem{
        .request_kind = OpenRequestKind::kUrlText,
        .target = "https://example.com/live-3.m3u8",
        .label = "live-3.m3u8",
    });
    settings.RememberRecentItem(RecentOpenItem{
        .request_kind = OpenRequestKind::kFilePath,
        .target = "/tmp/playlist-3.m3u",
        .label = "playlist-3.m3u",
    });
    settings.RememberRecentItem(RecentOpenItem{
        .request_kind = OpenRequestKind::kUrlText,
        .target = "https://example.com/live-2.m3u8",
        .label = "live-2.m3u8",
    });
    QVERIFY(settings.Save());

    AppSettings reloaded(config_path);
    QVERIFY(reloaded.Load());
    QCOMPARE(reloaded.RecentItems().size(), 5);
    QCOMPARE(EnumValue(reloaded.RecentItems().at(0).request_kind), EnumValue(OpenRequestKind::kUrlText));
    QCOMPARE(reloaded.RecentItems().at(0).target, QString("https://example.com/live-2.m3u8"));
    QCOMPARE(EnumValue(reloaded.RecentItems().at(1).request_kind), EnumValue(OpenRequestKind::kFilePath));
    QCOMPARE(reloaded.RecentItems().at(1).target, QString("/tmp/playlist-3.m3u"));
    QCOMPARE(reloaded.RecentItems().at(4).target, QString("/tmp/playlist-1.m3u"));
}

void LaunchOptionsTest::load_recent_items_deduplicates_existing_config_entries() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("config.toml");
    QFile seed_file(config_path);
    QVERIFY(seed_file.open(QIODevice::WriteOnly | QIODevice::Text));
    seed_file.write(
        "[network]\n"
        "user_agent = \"Seed Agent\"\n"
        "\n"
        "[[history.recent]]\n"
        "kind = \"url\"\n"
        "target = \"https://example.com/live-1.m3u8\"\n"
        "label = \"live-1.m3u8\"\n"
        "\n"
        "[[history.recent]]\n"
        "kind = \"file\"\n"
        "target = \"/tmp/playlist-1.m3u\"\n"
        "label = \"playlist-1.m3u\"\n"
        "\n"
        "[[history.recent]]\n"
        "kind = \"url\"\n"
        "target = \"https://example.com/live-1.m3u8\"\n"
        "label = \"live-1 duplicate\"\n"
        "\n"
        "[[history.recent]]\n"
        "kind = \"file\"\n"
        "target = \"/tmp/playlist-2.m3u\"\n"
        "label = \"playlist-2.m3u\"\n"
        "\n"
        "[[history.recent]]\n"
        "kind = \"url\"\n"
        "target = \"https://example.com/live-2.m3u8\"\n"
        "label = \"live-2.m3u8\"\n"
        "\n"
        "[[history.recent]]\n"
        "kind = \"file\"\n"
        "target = \"/tmp/playlist-3.m3u\"\n"
        "label = \"playlist-3.m3u\"\n");
    seed_file.close();

    AppSettings settings(config_path);
    QVERIFY(settings.Load());
    QCOMPARE(settings.UserAgent(), QString("Seed Agent"));
    QCOMPARE(settings.RecentItems().size(), 5);
    QCOMPARE(EnumValue(settings.RecentItems().at(0).request_kind), EnumValue(OpenRequestKind::kUrlText));
    QCOMPARE(settings.RecentItems().at(0).target, QString("https://example.com/live-1.m3u8"));
    QCOMPARE(EnumValue(settings.RecentItems().at(1).request_kind), EnumValue(OpenRequestKind::kFilePath));
    QCOMPARE(settings.RecentItems().at(1).target, QString("/tmp/playlist-1.m3u"));
    QCOMPARE(settings.RecentItems().at(2).target, QString("/tmp/playlist-2.m3u"));
    QCOMPARE(settings.RecentItems().at(3).target, QString("https://example.com/live-2.m3u8"));
    QCOMPARE(settings.RecentItems().at(4).target, QString("/tmp/playlist-3.m3u"));
}

void LaunchOptionsTest::load_invalid_osd_auto_hide_seconds_falls_back_to_default() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("config.toml");
    QFile seed_file(config_path);
    QVERIFY(seed_file.open(QIODevice::WriteOnly | QIODevice::Text));
    seed_file.write(
        "[ui.osd]\n"
        "auto_hide_seconds = 0\n");
    seed_file.close();

    AppSettings settings(config_path);
    QVERIFY(settings.Load());
    QCOMPARE(settings.OsdAutoHideSeconds(), 3);
}

void LaunchOptionsTest::save_settings_preserves_existing_comments_and_fields() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString config_path = temp_dir.filePath("config.toml");
    QFile seed_file(config_path);
    QVERIFY(seed_file.open(QIODevice::WriteOnly | QIODevice::Text));
    seed_file.write("# existing comment\n"
                    "[network]\n"
                    "user_agent = \"Old Agent\"\n"
                    "\n"
                    "[ui]\n"
                    "layout = \"classic\"\n");
    seed_file.close();

    AppSettings settings(config_path);
    QVERIFY(settings.Load());
    settings.SetUserAgent("ShaTV Desktop/2.0");
    QVERIFY(settings.Save());

    QVERIFY(seed_file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString file_text = QString::fromUtf8(seed_file.readAll());
    seed_file.close();

    QVERIFY(file_text.contains("# existing comment"));
    QVERIFY(file_text.contains("layout = \"classic\""));
    QVERIFY(file_text.contains("user_agent = \"ShaTV Desktop/2.0\""));
}

}  // namespace

QTEST_GUILESS_MAIN(LaunchOptionsTest)

#include "launch_options_test.moc"
