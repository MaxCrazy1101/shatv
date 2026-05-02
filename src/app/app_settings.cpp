#include "app/app_settings.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <toml.hpp>

namespace shatv::app {

namespace {

std::string ToStdString(const QString &value) {
    return value.toUtf8().toStdString();
}

constexpr std::size_t kMaxRecentItems = 5;
constexpr int kDefaultOsdAutoHideSeconds = 3;
constexpr int kDefaultVolume = 50;
constexpr bool kDefaultMuted = false;

bool IsSameRecentItem(const RecentOpenItem &lhs, const RecentOpenItem &rhs) {
    return lhs.request_kind == rhs.request_kind && lhs.target == rhs.target;
}

void NormalizeRecentItems(std::vector<RecentOpenItem> &items) {
    std::vector<RecentOpenItem> normalized;
    normalized.reserve(std::min(items.size(), kMaxRecentItems));

    for (auto &item : items) {
        if (item.target.isEmpty()) {
            continue;
        }
        if (item.label.isEmpty()) {
            item.label = item.target;
        }
        if (std::any_of(normalized.begin(), normalized.end(),
                        [&item](const RecentOpenItem &existing) { return IsSameRecentItem(existing, item); })) {
            continue;
        }
        normalized.push_back(std::move(item));
        if (normalized.size() >= kMaxRecentItems) {
            break;
        }
    }

    items = std::move(normalized);
}

int NormalizeOsdAutoHideSeconds(const toml::value &config, const QString &config_path) {
    if (!config.contains("ui")) {
        return kDefaultOsdAutoHideSeconds;
    }

    const auto &ui = config.at("ui");
    if (!ui.is_table() || !ui.contains("osd")) {
        return kDefaultOsdAutoHideSeconds;
    }

    const auto &osd = ui.at("osd");
    if (!osd.is_table()) {
        std::cerr << "ShaTV config invalid ui.osd path=" << config_path.toStdString() << '\n';
        return kDefaultOsdAutoHideSeconds;
    }

    try {
        const int seconds = toml::find_or<int>(osd, "auto_hide_seconds", kDefaultOsdAutoHideSeconds);
        if (seconds >= 1) {
            return seconds;
        }

        std::cerr << "ShaTV config invalid ui.osd.auto_hide_seconds path="
                  << config_path.toStdString()
                  << " value=" << seconds << '\n';
        return kDefaultOsdAutoHideSeconds;
    } catch (const std::exception &) {
        std::cerr << "ShaTV config invalid ui.osd.auto_hide_seconds path="
                  << config_path.toStdString() << '\n';
        return kDefaultOsdAutoHideSeconds;
    }
}

std::pair<int, bool> NormalizePlaybackState(const toml::value &config, const QString &config_path) {
    int volume = kDefaultVolume;
    bool muted = kDefaultMuted;

    if (!config.contains("playback")) {
        return {volume, muted};
    }

    const auto &playback = config.at("playback");
    if (!playback.is_table()) {
        std::cerr << "ShaTV config invalid playback section path=" << config_path.toStdString() << '\n';
        return {volume, muted};
    }

    try {
        volume = toml::find_or<int>(playback, "volume", kDefaultVolume);
        if (volume < 0 || volume > 100) {
            std::cerr << "ShaTV config invalid playback.volume path=" << config_path.toStdString()
                      << " value=" << volume << '\n';
            volume = kDefaultVolume;
        }
        muted = toml::find_or<bool>(playback, "muted", kDefaultMuted);
    } catch (const std::exception &) {
        std::cerr << "ShaTV config invalid playback section path=" << config_path.toStdString() << '\n';
    }

    return {volume, muted};
}

}  // namespace

AppSettings::AppSettings(QString config_path) : config_path_(std::move(config_path)) {}

QString AppSettings::DefaultConfigPath() {
    const QString config_root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(config_root).filePath("config.toml");
}

const QString &AppSettings::ConfigPath() const {
    return config_path_;
}

const QString &AppSettings::EpgUrl() const {
    return epg_url_;
}

const QString &AppSettings::UserAgent() const {
    return user_agent_;
}

int AppSettings::OsdAutoHideSeconds() const {
    return osd_auto_hide_seconds_;
}

int AppSettings::Volume() const {
    return volume_;
}

bool AppSettings::Muted() const {
    return muted_;
}

const std::vector<RecentOpenItem> &AppSettings::RecentItems() const {
    return recent_items_;
}

void AppSettings::SetEpgUrl(const QString &epg_url) {
    epg_url_ = epg_url.trimmed();
}

void AppSettings::SetUserAgent(const QString &user_agent) {
    user_agent_ = user_agent;
}

void AppSettings::SetOsdAutoHideSeconds(int seconds) {
    if (seconds >= 1) {
        osd_auto_hide_seconds_ = seconds;
    }
}

void AppSettings::SetVolume(int volume) {
    if (volume >= 0 && volume <= 100) {
        volume_ = volume;
    }
}

void AppSettings::SetMuted(bool muted) {
    muted_ = muted;
}

void AppSettings::RememberRecentItem(RecentOpenItem item) {
    if (item.target.isEmpty()) {
        return;
    }

    if (item.label.isEmpty()) {
        item.label = item.target;
    }

    recent_items_.erase(std::remove_if(recent_items_.begin(), recent_items_.end(),
                                       [&item](const RecentOpenItem &existing) {
                                           return IsSameRecentItem(existing, item);
                                       }),
                        recent_items_.end());
    recent_items_.insert(recent_items_.begin(), std::move(item));
    if (recent_items_.size() > kMaxRecentItems) {
        recent_items_.resize(kMaxRecentItems);
    }
}

bool AppSettings::Load() {
    if (!QFileInfo::exists(config_path_)) {
        epg_url_.clear();
        user_agent_.clear();
        osd_auto_hide_seconds_ = kDefaultOsdAutoHideSeconds;
        volume_ = kDefaultVolume;
        muted_ = kDefaultMuted;
        recent_items_.clear();
        return true;
    }

    try {
        const toml::value config = toml::parse(ToStdString(config_path_));
        epg_url_ = QString::fromStdString(
            toml::find_or<std::string>(config, "epg", "url", std::string()));
        user_agent_ = QString::fromStdString(
            toml::find_or<std::string>(config, "network", "user_agent", std::string()));
        osd_auto_hide_seconds_ = NormalizeOsdAutoHideSeconds(config, config_path_);
        std::tie(volume_, muted_) = NormalizePlaybackState(config, config_path_);
        recent_items_.clear();

        if (config.contains("history")) {
            const auto &history = config.at("history");
            if (history.is_table() && history.contains("recent")) {
                const auto &recent = history.at("recent");
                if (recent.is_array()) {
                    for (const auto &entry : recent.as_array()) {
                        if (!entry.is_table()) {
                            continue;
                        }

                        const QString request_kind =
                            QString::fromStdString(toml::find_or<std::string>(entry, "request_kind", std::string()));
                        const QString legacy_kind =
                            QString::fromStdString(toml::find_or<std::string>(entry, "kind", std::string()));
                        const QString target =
                            QString::fromStdString(toml::find_or<std::string>(entry, "target", std::string()));
                        const QString label =
                            QString::fromStdString(toml::find_or<std::string>(entry, "label", std::string()));
                        const std::optional<OpenRequestKind> parsed_request_kind =
                            request_kind.isEmpty() ? LegacyOpenRequestKindFromToken(legacy_kind)
                                                   : OpenRequestKindFromToken(request_kind);
                        if (!parsed_request_kind.has_value() || target.isEmpty()) {
                            continue;
                        }
                        recent_items_.push_back(RecentOpenItem{
                            .request_kind = *parsed_request_kind,
                            .target = target,
                            .label = label.isEmpty() ? target : label,
                        });
                    }
                }
            }
        }

        NormalizeRecentItems(recent_items_);
        return true;
    } catch (const std::exception &) {
        return false;
    }
}

bool AppSettings::Save() const {
    QFileInfo config_info(config_path_);
    QDir config_dir = config_info.dir();
    if (!config_dir.exists() && !config_dir.mkpath(".")) {
        return false;
    }

    try {
        toml::value config;
        if (config_info.exists()) {
            config = toml::parse(ToStdString(config_path_));
        }

        // 配置层只维护一个极小 TOML 结构，避免把平台特定设置后端耦合进来。
        config["epg"]["url"] = toml::value(ToStdString(epg_url_));
        config["network"]["user_agent"] = toml::value(ToStdString(user_agent_));
        config["ui"]["osd"]["auto_hide_seconds"] = toml::value(osd_auto_hide_seconds_);
        config["playback"]["volume"] = toml::value(volume_);
        config["playback"]["muted"] = toml::value(muted_);
        toml::array recent_entries;
        for (const auto &item : recent_items_) {
            toml::value entry;
            entry["request_kind"] = toml::value(ToStdString(OpenRequestKindToken(item.request_kind)));
            entry["target"] = toml::value(ToStdString(item.target));
            entry["label"] = toml::value(ToStdString(item.label));
            recent_entries.push_back(std::move(entry));
        }
        config["history"]["recent"] = toml::value(std::move(recent_entries));

        std::ofstream output(ToStdString(config_path_), std::ios::trunc);
        if (!output.is_open()) {
            return false;
        }
        output << toml::format(config);
        output.flush();
        return output.good();
    } catch (const std::exception &) {
        return false;
    }
}

}  // namespace shatv::app
