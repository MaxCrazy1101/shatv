#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QApplication>
#include <QString>

#include "app/app_settings.h"
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
    void OpenChannels(std::vector<domain::Channel> channels);
    void OpenFile(const QString &path);
    void OpenPlaylistFile(const QString &path);
    void OpenUrl(const QString &url_text);
    void DownloadPlaylist(const QUrl &url);
    void ShowPlaylistImportError(const QString &message);
    void UpdateNetworkUserAgent(const QString &user_agent);
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
    std::vector<domain::Channel> demo_channels_;
    std::optional<domain::Channel> startup_channel_;
};

}  // namespace shatv::app
