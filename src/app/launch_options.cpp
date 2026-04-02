#include "app/launch_options.h"

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
    options.smoke_test = arguments.contains("--smoke-test");
    options.mpv_smoke = arguments.contains("--mpv-smoke");
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
        .group = "Open URL",
    };
}

domain::Channel BuildOpenMediaChannel(const QString &input, const QString &current_directory) {
    const QUrl media_url = QUrl::fromUserInput(input, current_directory, QUrl::AssumeLocalFile);
    return domain::Channel{
        .id = "open-media",
        .name = ChannelNameFromUrl(media_url),
        .url = media_url,
        .group = "Open Media",
    };
}

bool IsRemotePlaybackUrl(const QUrl &url) {
    if (!url.isValid() || url.isLocalFile()) {
        return false;
    }

    const QString scheme = url.scheme().toLower();
    return scheme == "http" || scheme == "https";
}

std::optional<domain::Channel> BuildStartupChannel(const LaunchOptions &options, const QString &smoke_media,
                                                   const QString &current_directory) {
    if (options.mpv_smoke && !smoke_media.isEmpty()) {
        const QString absolute_path = QFileInfo(smoke_media).absoluteFilePath();
        return domain::Channel{
            .id = "demo-news",
            .name = QFileInfo(absolute_path).fileName(),
            .url = QUrl::fromLocalFile(absolute_path),
            .group = "Smoke",
        };
    }

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
