#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QApplication>

#include "app/launch_options.h"
#include "domain/channel.h"

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
    void SetupSmokeScenario();
    void SetupMpvSmokeScenario();

    QApplication *qt_app_ = nullptr;
    LaunchOptions options_;
    bool smoke_completed_ = false;
    std::unique_ptr<player::PlayerBackend> backend_;
    std::unique_ptr<application::PlayerController> controller_;
    std::unique_ptr<ui::models::ChannelListModel> channel_model_;
    std::unique_ptr<ui::windows::MainWindow> main_window_;
    std::vector<domain::Channel> demo_channels_;
    std::optional<domain::Channel> startup_channel_;
};

}  // namespace shatv::app
