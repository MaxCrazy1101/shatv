#pragma once

#include <memory>
#include <vector>

#include <QApplication>

#include "domain/channel.h"

namespace shatv::application {
class PlayerController;
}

namespace shatv::player {
class FakePlayerBackend;
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
    Application(QApplication *qt_app, bool smoke_test);
    ~Application();
    int Run();

   private:
    std::vector<domain::Channel> BuildDemoChannels() const;
    void SetupSmokeScenario();

    QApplication *qt_app_ = nullptr;
    bool smoke_test_ = false;
    bool smoke_completed_ = false;
    std::unique_ptr<player::FakePlayerBackend> backend_;
    std::unique_ptr<application::PlayerController> controller_;
    std::unique_ptr<ui::models::ChannelListModel> channel_model_;
    std::unique_ptr<ui::windows::MainWindow> main_window_;
    std::vector<domain::Channel> demo_channels_;
};

}  // namespace shatv::app
