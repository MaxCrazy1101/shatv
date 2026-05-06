#include <cstddef>
#include <optional>
#include <variant>

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QNetworkAccessManager>
#include <QTemporaryDir>

#include "app/source_open_service.h"

namespace {

using shatv::app::ChannelListResolution;
using shatv::app::DirectMediaResolution;
using shatv::app::OpenErrorResolution;
using shatv::app::OpenRequest;
using shatv::app::OpenRequestKind;
using shatv::app::OpenResolution;
using shatv::app::SourceOpenContext;
using shatv::app::SourceOpenService;
using shatv::domain::RetryBackoff;
using shatv::domain::SourceKind;
using shatv::domain::SourceOrigin;

template <typename Enum>
int EnumValue(Enum value) {
    return static_cast<int>(value);
}

class SourceOpenServiceTest final : public QObject {
    Q_OBJECT

   private slots:
    void resolves_local_media_file_to_direct_descriptor();
    void resolves_direct_remote_media_with_user_agent_and_retry_policy();
    void resolves_local_playlist_to_descriptor_backed_channels();
    void rejects_remote_root_url();
    void resolves_recent_url_through_replay_request_kind();
};

std::optional<OpenResolution> ResolveSync(SourceOpenService &service,
                                          OpenRequest request,
                                          const SourceOpenContext &context) {
    std::optional<OpenResolution> result;
    service.Resolve(std::move(request), context, [&result](OpenResolution resolution) {
        result = std::move(resolution);
    });
    return result;
}

void SourceOpenServiceTest::resolves_local_media_file_to_direct_descriptor() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString media_path = temp_dir.filePath("clip.mp4");
    QFile media_file(media_path);
    QVERIFY(media_file.open(QIODevice::WriteOnly));
    media_file.close();

    QNetworkAccessManager network_manager;
    SourceOpenService service(&network_manager);
    const std::optional<OpenResolution> resolution = ResolveSync(
        service,
        OpenRequest{
            .request_kind = OpenRequestKind::kFilePath,
            .target = media_path,
            .label = {},
            .replay_request_kind = std::nullopt,
        },
        SourceOpenContext{
            .current_directory = temp_dir.path(),
            .user_agent = "ShaTV Test/1.0",
        });

    QVERIFY(resolution.has_value());
    const auto *direct = std::get_if<DirectMediaResolution>(&*resolution);
    QVERIFY(direct != nullptr);
    QCOMPARE(direct->item.channel.name, QString("clip.mp4"));
    QCOMPARE(direct->item.source.id, QString("open-media"));
    QCOMPARE(EnumValue(direct->item.source.source_kind), EnumValue(SourceKind::kLocalFile));
    QCOMPARE(EnumValue(direct->item.source.origin), EnumValue(SourceOrigin::kManualOpenFile));
    QCOMPARE(direct->item.source.user_agent, QString());
    QCOMPARE(direct->item.source.retry_policy.max_attempts, 0);
    QVERIFY(direct->recent_item.has_value());
    QCOMPARE(EnumValue(direct->recent_item->request_kind), EnumValue(OpenRequestKind::kFilePath));
    QCOMPARE(direct->recent_item->target, media_path);
}

void SourceOpenServiceTest::resolves_direct_remote_media_with_user_agent_and_retry_policy() {
    QNetworkAccessManager network_manager;
    SourceOpenService service(&network_manager);
    const std::optional<OpenResolution> resolution = ResolveSync(
        service,
        OpenRequest{
            .request_kind = OpenRequestKind::kUrlText,
            .target = "https://example.com/live.m3u8",
            .label = {},
            .replay_request_kind = std::nullopt,
        },
        SourceOpenContext{
            .current_directory = QDir::currentPath(),
            .user_agent = "ShaTV Test/1.0",
        });

    QVERIFY(resolution.has_value());
    const auto *direct = std::get_if<DirectMediaResolution>(&*resolution);
    QVERIFY(direct != nullptr);
    QCOMPARE(EnumValue(direct->item.source.source_kind), EnumValue(SourceKind::kDirectRemoteMedia));
    QCOMPARE(EnumValue(direct->item.source.origin), EnumValue(SourceOrigin::kManualOpenUrl));
    QCOMPARE(direct->item.source.user_agent, QString("ShaTV Test/1.0"));
    QCOMPARE(direct->item.source.retry_policy.max_attempts, 2);
    QCOMPARE(direct->item.source.retry_policy.initial_delay_ms, 300);
    QCOMPARE(direct->item.source.retry_policy.max_delay_ms, 1000);
    QCOMPARE(EnumValue(direct->item.source.retry_policy.backoff), EnumValue(RetryBackoff::kFixed));
    QVERIFY(direct->recent_item.has_value());
    QCOMPARE(EnumValue(direct->recent_item->request_kind), EnumValue(OpenRequestKind::kUrlText));
    QCOMPARE(direct->recent_item->target, QString("https://example.com/live.m3u8"));
}

void SourceOpenServiceTest::resolves_local_playlist_to_descriptor_backed_channels() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString playlist_path = temp_dir.filePath("iptv.m3u");
    QFile playlist_file(playlist_path);
    QVERIFY(playlist_file.open(QIODevice::WriteOnly | QIODevice::Text));
    playlist_file.write(
        "#EXTM3U x-tvg-url=\"https://epg.example.com/guide.xml\"\n"
        "#EXTINF:-1 tvg-id=\"news\" tvg-name=\"News\" group-title=\"General\",News Live\n"
        "https://media.example.com/news.m3u8\n");
    playlist_file.close();

    QNetworkAccessManager network_manager;
    SourceOpenService service(&network_manager);
    const std::optional<OpenResolution> resolution = ResolveSync(
        service,
        OpenRequest{
            .request_kind = OpenRequestKind::kFilePath,
            .target = playlist_path,
            .label = {},
            .replay_request_kind = std::nullopt,
        },
        SourceOpenContext{
            .current_directory = temp_dir.path(),
            .user_agent = "ShaTV Test/1.0",
        });

    QVERIFY(resolution.has_value());
    const auto *channel_list = std::get_if<ChannelListResolution>(&*resolution);
    QVERIFY(channel_list != nullptr);
    QCOMPARE(channel_list->channels.size(), static_cast<std::size_t>(1));
    QCOMPARE(channel_list->playlist_epg_url, QString("https://epg.example.com/guide.xml"));
    QVERIFY(channel_list->recent_item.has_value());
    QCOMPARE(EnumValue(channel_list->recent_item->request_kind), EnumValue(OpenRequestKind::kFilePath));
    QCOMPARE(channel_list->recent_item->target, playlist_path);

    const auto &resolved_channel = channel_list->channels.at(0);
    QCOMPARE(resolved_channel.channel.name, QString("News Live"));
    QCOMPARE(resolved_channel.source.id, resolved_channel.channel.id);
    QCOMPARE(EnumValue(resolved_channel.source.source_kind), EnumValue(SourceKind::kPlaylistChannelLive));
    QCOMPARE(EnumValue(resolved_channel.source.origin), EnumValue(SourceOrigin::kLocalPlaylist));
    QCOMPARE(resolved_channel.source.user_agent, QString("ShaTV Test/1.0"));
    QCOMPARE(resolved_channel.source.retry_policy.max_attempts, 5);
    QCOMPARE(resolved_channel.source.retry_policy.initial_delay_ms, 300);
    QCOMPARE(resolved_channel.source.retry_policy.max_delay_ms, 3000);
    QCOMPARE(EnumValue(resolved_channel.source.retry_policy.backoff), EnumValue(RetryBackoff::kIncreasing));
}

void SourceOpenServiceTest::rejects_remote_root_url() {
    QNetworkAccessManager network_manager;
    SourceOpenService service(&network_manager);
    const std::optional<OpenResolution> resolution = ResolveSync(
        service,
        OpenRequest{
            .request_kind = OpenRequestKind::kUrlText,
            .target = "https://example.com/",
            .label = {},
            .replay_request_kind = std::nullopt,
        },
        SourceOpenContext{
            .current_directory = QDir::currentPath(),
            .user_agent = "ShaTV Test/1.0",
        });

    QVERIFY(resolution.has_value());
    const auto *error = std::get_if<OpenErrorResolution>(&*resolution);
    QVERIFY(error != nullptr);
    QVERIFY(error->message.contains("full media URL"));
}

void SourceOpenServiceTest::resolves_recent_url_through_replay_request_kind() {
    QNetworkAccessManager network_manager;
    SourceOpenService service(&network_manager);
    const std::optional<OpenResolution> resolution = ResolveSync(
        service,
        OpenRequest{
            .request_kind = OpenRequestKind::kRecentItem,
            .target = "https://example.com/recent-live.m3u8",
            .label = {},
            .replay_request_kind = OpenRequestKind::kUrlText,
        },
        SourceOpenContext{
            .current_directory = QDir::currentPath(),
            .user_agent = "ShaTV Test/1.0",
        });

    QVERIFY(resolution.has_value());
    const auto *direct = std::get_if<DirectMediaResolution>(&*resolution);
    QVERIFY(direct != nullptr);
    QCOMPARE(EnumValue(direct->item.source.source_kind), EnumValue(SourceKind::kDirectRemoteMedia));
    QCOMPARE(EnumValue(direct->item.source.origin), EnumValue(SourceOrigin::kRecentItem));
    QCOMPARE(direct->item.source.user_agent, QString("ShaTV Test/1.0"));
    QVERIFY(direct->recent_item.has_value());
    QCOMPARE(EnumValue(direct->recent_item->request_kind), EnumValue(OpenRequestKind::kUrlText));
    QCOMPARE(direct->recent_item->target, QString("https://example.com/recent-live.m3u8"));
}

}  // namespace

QTEST_GUILESS_MAIN(SourceOpenServiceTest)

#include "source_open_service_test.moc"
