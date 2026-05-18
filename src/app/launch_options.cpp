#include "app/launch_options.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QUrl>

namespace shatv::app {

namespace {

QString FindOptionValue(const QStringList &arguments, const QString &option_name) {
    for (int index = 0; index < arguments.size(); ++index) {
        const QString &argument = arguments.at(index);
        if (argument == option_name && index + 1 < arguments.size()) {
            return arguments.at(index + 1);
        }

        const QString prefix = option_name + "=";
        if (argument.startsWith(prefix)) {
            return argument.mid(prefix.size());
        }
    }

    return {};
}

QString ChannelNameFromUrl(const QUrl &url) {
    if (url.isLocalFile()) {
        const QString file_name = QFileInfo(url.toLocalFile()).fileName();
        if (!file_name.isEmpty()) {
            return file_name;
        }
    }

    const QString path_name = QFileInfo(url.path()).fileName();
    if (!path_name.isEmpty()) {
        return path_name;
    }

    if (!url.host().isEmpty()) {
        return url.host();
    }

    return url.toString();
}

}  // namespace

LaunchOptions ParseLaunchOptions(const QStringList &arguments) {
    LaunchOptions options;
    options.ffmpeg_audio_smoke = arguments.contains("--ffmpeg-audio-smoke");
    options.ffmpeg_smoke = arguments.contains("--ffmpeg-smoke");
    options.open_url_argument = FindOptionValue(arguments, "--open-url");
    options.open_media_argument = FindOptionValue(arguments, "--open-media");
    return options;
}

domain::Channel BuildOpenUrlChannel(const QString &input, const QString &current_directory) {
    const QUrl media_url = QUrl::fromUserInput(input, current_directory);
    return domain::Channel{
        .id = "open-url",
        .name = ChannelNameFromUrl(media_url),
        .url = media_url,
        .group = QCoreApplication::translate("LaunchOptions", "Open URL"),
        .tvg_id = {},
        .tvg_name = {},
    };
}

domain::Channel BuildOpenMediaChannel(const QString &input, const QString &current_directory) {
    const QUrl media_url = QUrl::fromUserInput(input, current_directory, QUrl::AssumeLocalFile);
    return domain::Channel{
        .id = "open-media",
        .name = ChannelNameFromUrl(media_url),
        .url = media_url,
        .group = QCoreApplication::translate("LaunchOptions", "Open Media"),
        .tvg_id = {},
        .tvg_name = {},
    };
}

bool IsRemotePlaybackUrl(const QUrl &url) {
    if (!url.isValid() || url.isLocalFile()) {
        return false;
    }

    const QString scheme = url.scheme().toLower();
    return scheme == "http" || scheme == "https";
}

bool LooksLikeRemoteMediaDirectoryUrl(const QUrl &url) {
    if (!IsRemotePlaybackUrl(url)) {
        return false;
    }

    const QString path = url.path();
    if (!(path.isEmpty() || path == "/")) {
        return false;
    }

    return !url.hasQuery();
}

std::optional<domain::Channel> BuildStartupChannel(const LaunchOptions &options, const QString &current_directory) {
    // 启动参数冲突时优先使用显式 URL，避免被本地媒体参数覆盖。
    if (!options.open_url_argument.isEmpty()) {
        return BuildOpenUrlChannel(options.open_url_argument, current_directory);
    }

    if (options.open_media_argument.isEmpty()) {
        return std::nullopt;
    }

    return BuildOpenMediaChannel(options.open_media_argument, current_directory);
}

}  // namespace shatv::app
