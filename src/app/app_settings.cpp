#include "app/app_settings.h"

#include <fstream>
#include <stdexcept>
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

}  // namespace

AppSettings::AppSettings(QString config_path) : config_path_(std::move(config_path)) {}

QString AppSettings::DefaultConfigPath() {
    const QString config_root = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(config_root).filePath("config.toml");
}

const QString &AppSettings::ConfigPath() const {
    return config_path_;
}

const QString &AppSettings::UserAgent() const {
    return user_agent_;
}

void AppSettings::SetUserAgent(const QString &user_agent) {
    user_agent_ = user_agent;
}

bool AppSettings::Load() {
    if (!QFileInfo::exists(config_path_)) {
        user_agent_.clear();
        return true;
    }

    try {
        const toml::value config = toml::parse(ToStdString(config_path_));
        user_agent_ = QString::fromStdString(
            toml::find_or<std::string>(config, "network", "user_agent", std::string()));
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
        config["network"]["user_agent"] = toml::value(ToStdString(user_agent_));

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
