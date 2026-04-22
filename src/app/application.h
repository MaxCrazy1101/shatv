#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QGuiApplication>
#include <QPointer>
#include <QString>
#include <QTimer>

#include "app/app_settings.h"
#include "app/epg_service.h"
#include "app/launch_options.h"
#include "domain/channel.h"

class QNetworkAccessManager;
class QObject;
class QQmlApplicationEngine;
class QWindow;

namespace shatv::application {
class PlayerController;
}

namespace shatv::player {
class FakePlayerBackend;
class PlayerBackend;
}

namespace shatv::ui::models {
class ChannelFilterModel;
class ChannelListModel;
}

namespace shatv::ui::shell {
class AppShellBridge;
}

namespace shatv::ui::video {
class MpvVideoItem;
}

namespace shatv::app {

class Application final {
   public:
    Application(QGuiApplication *qt_app, LaunchOptions options);
    ~Application();
    int Run();

   private:
    std::vector<domain::Channel> BuildInitialChannels() const;
    void OpenChannel(const domain::Channel &channel);
    void OpenChannels(std::vector<domain::Channel> channels, const QString &playlist_epg_url = QString());
    void OpenFile(const QString &path);
    void OpenPlaylistFile(const QString &path);
    void OpenUrl(const QString &url_text);
    void DownloadPlaylist(const QUrl &url);
    void ShowPlaylistImportError(const QString &message);
    void UpdateNetworkSettings(const QString &user_agent, const QString &epg_url);
    void ShowAlert(const QString &message);
    void ReloadEpg();
    void UpdateDisplayedEpg();
    void ClearDisplayedEpg();
    std::optional<domain::Channel> FindChannelById(const QString &channel_id) const;
    void RememberRecentItem(const RecentOpenItem &item);
    void RefreshRecentItems();
    void RefreshShellFilters();
    void ShowStatusMessage(const QString &message, int timeout_ms = 3000);
    void OpenRecentItem(const QString &kind, const QString &target);
    void StartInitialPlayback();
    void SetupSmokeScenario();
    void SetupMpvSmokeScenario();

    QGuiApplication *qt_app_ = nullptr;
    LaunchOptions options_;
    AppSettings settings_;
    bool smoke_completed_ = false;
    std::unique_ptr<QNetworkAccessManager> network_manager_;
    std::unique_ptr<player::PlayerBackend> backend_;
    std::unique_ptr<application::PlayerController> controller_;
    std::unique_ptr<ui::models::ChannelListModel> channel_model_;
    std::unique_ptr<ui::models::ChannelFilterModel> channel_filter_model_;
    std::unique_ptr<ui::shell::AppShellBridge> shell_bridge_;
    std::unique_ptr<QQmlApplicationEngine> qml_engine_;
    QPointer<QObject> qml_root_object_;
    QPointer<QWindow> root_window_;
    QPointer<ui::video::MpvVideoItem> video_item_;
    EpgService epg_service_;
    std::vector<domain::Channel> initial_channels_;
    std::vector<domain::Channel> current_channels_;
    std::optional<domain::Channel> startup_channel_;
    QString playlist_epg_url_;
    QString status_message_;
    QTimer epg_refresh_timer_;
    QTimer status_message_timer_;
    int epg_load_generation_ = 0;
};

}  // namespace shatv::app
