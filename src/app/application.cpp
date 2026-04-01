#include "app/application.h"

#include <iostream>

#include <QCoreApplication>
#include <QTimer>

#include "application/player_controller.h"
#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/fake_player_backend.h"
#include "ui/models/channel_list_model.h"
#include "ui/windows/main_window.h"

namespace shatv::app {

Application::Application(QApplication *qt_app, bool smoke_test) : qt_app_(qt_app), smoke_test_(smoke_test) {
    Q_ASSERT(qt_app_ != nullptr);

    qRegisterMetaType<domain::PlayerSnapshot>("shatv::domain::PlayerSnapshot");

    backend_ = std::make_unique<player::FakePlayerBackend>();
    controller_ = std::make_unique<application::PlayerController>(backend_.get());
    channel_model_ = std::make_unique<ui::models::ChannelListModel>();
    main_window_ = std::make_unique<ui::windows::MainWindow>(controller_.get(), channel_model_.get());

    demo_channels_ = BuildDemoChannels();
    main_window_->SetChannels(demo_channels_);
}

Application::~Application() = default;

int Application::Run() {
    main_window_->show();

    if (smoke_test_) {
        std::cout << "ShaTV Qt6 bootstrap" << std::endl;
        SetupSmokeScenario();
    }

    return qt_app_->exec();
}

std::vector<domain::Channel> Application::BuildDemoChannels() const {
    return {
        {.id = "demo-news", .name = "Demo News", .url = QUrl("https://example.com/demo-news.m3u8"), .group = "News"},
        {.id = "demo-sports",
         .name = "Demo Sports",
         .url = QUrl("https://example.com/demo-sports.m3u8"),
         .group = "Sports"},
        {.id = "demo-movies",
         .name = "Demo Movies",
         .url = QUrl("https://example.com/demo-movies.m3u8"),
         .group = "Movies"},
    };
}

void Application::SetupSmokeScenario() {
    QObject::connect(main_window_.get(), &ui::windows::MainWindow::UiSnapshotApplied, qt_app_,
                     [this](const domain::PlayerSnapshot &snapshot) {
                         if (smoke_completed_ || snapshot.state != domain::PlaybackState::kPlaying) {
                             return;
                         }

                         smoke_completed_ = true;
                         const QString state_name =
                             domain::PlaybackStateName(main_window_->LastAppliedSnapshot().state).toLower();

                         std::cout << "ShaTV Stage2 smoke ok channels=" << main_window_->ChannelCount()
                                   << " current=" << main_window_->CurrentChannelIdForSmoke().toStdString()
                                   << " state=" << state_name.toStdString() << std::endl;

                         QTimer::singleShot(0, qt_app_, &QCoreApplication::quit);
                     });

    // 通过定时触发模拟一次真实的首频道点击，验证 UI -> Controller -> Backend -> UI 主链。
    QTimer::singleShot(0, main_window_.get(), &ui::windows::MainWindow::StartSmokeScenario);
}

}  // namespace shatv::app
