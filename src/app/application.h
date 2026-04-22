#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QApplication>
#include <QString>
#include <QTimer>

#include "app/app_settings.h"
#include "app/epg_service.h"
#include "app/launch_options.h"
#include "domain/channel.h"

class QNetworkAccessManager;

namespace shatv::application {
class PlayerController;
}

namespace shatv::player {
class FakePlayerBackend;
class PlayerBackend;
}

namespace shatv::ui::models {
class ChannelListModel;
}

namespace shatv::ui::windows {
class MainWindow;
}

namespace shatv::app {

class Application final {
   public:
    Application(QApplication *qt_app, LaunchOptions options);
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
    void ReloadEpg();
    void UpdateDisplayedEpg();
    void ClearDisplayedEpg();
    std::optional<domain::Channel> FindChannelById(const QString &channel_id) const;
    void RememberRecentItem(const RecentOpenItem &item);
    void RefreshRecentItems();
    void OpenRecentItem(const QString &kind, const QString &target);
    void SetupSmokeScenario();
    void SetupMpvSmokeScenario();

    QApplication *qt_app_ = nullptr;
    LaunchOptions options_;
    AppSettings settings_;
    bool smoke_completed_ = false;
    std::unique_ptr<QNetworkAccessManager> network_manager_;
    std::unique_ptr<player::PlayerBackend> backend_;
    std::unique_ptr<application::PlayerController> controller_;
    std::unique_ptr<ui::models::ChannelListModel> channel_model_;
    std::unique_ptr<ui::windows::MainWindow> main_window_;
    EpgService epg_service_;
    std::vector<domain::Channel> initial_channels_;
    std::vector<domain::Channel> current_channels_;
    std::optional<domain::Channel> startup_channel_;
    QString playlist_epg_url_;
    QTimer epg_refresh_timer_;
    int epg_load_generation_ = 0;
};

}  // namespace shatv::app
