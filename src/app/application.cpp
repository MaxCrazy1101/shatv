#include "app/application.h"

#include <iostream>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include "application/player_controller.h"
#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/fake_player_backend.h"
#include "player/mpv_player_backend.h"
#include "player/mpv_render_widget.h"
#include "ui/models/channel_list_model.h"
#include "ui/windows/main_window.h"

namespace shatv::app {

Application::Application(QApplication *qt_app, LaunchOptions options)
    : qt_app_(qt_app), options_(std::move(options)) {
    Q_ASSERT(qt_app_ != nullptr);

    qRegisterMetaType<domain::PlayerSnapshot>("shatv::domain::PlayerSnapshot");

    if (options_.smoke_test) {
        backend_ = std::make_unique<player::FakePlayerBackend>();
    } else {
        backend_ = std::make_unique<player::MpvPlayerBackend>();
    }
    controller_ = std::make_unique<application::PlayerController>(backend_.get());
    channel_model_ = std::make_unique<ui::models::ChannelListModel>();
    main_window_ = std::make_unique<ui::windows::MainWindow>(controller_.get(), channel_model_.get());

    if (auto *mpv_backend = dynamic_cast<player::MpvPlayerBackend *>(backend_.get())) {
        mpv_backend->AttachRenderWidget(main_window_->RenderWidget());
        main_window_->RenderWidget()->SetBackend(mpv_backend);
    }

    startup_channel_ = BuildStartupChannel(options_, qEnvironmentVariable("SHATV_SMOKE_MEDIA"), QDir::currentPath());
    demo_channels_ = BuildInitialChannels();
    main_window_->SetChannels(demo_channels_);
}

Application::~Application() {
    controller_.reset();
    backend_.reset();
    main_window_.reset();
    channel_model_.reset();
}

int Application::Run() {
    main_window_->show();

    if (options_.smoke_test) {
        std::cout << "ShaTV Qt6 bootstrap" << std::endl;
        SetupSmokeScenario();
    }
    if (options_.mpv_smoke) {
        SetupMpvSmokeScenario();
    } else if (startup_channel_.has_value()) {
        QTimer::singleShot(0, main_window_.get(), &ui::windows::MainWindow::StartInitialPlayback);
    }

    return qt_app_->exec();
}

std::vector<domain::Channel> Application::BuildInitialChannels() const {
    if (startup_channel_.has_value()) {
        return {*startup_channel_};
    }

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

void Application::SetupMpvSmokeScenario() {
    QObject::connect(main_window_.get(), &ui::windows::MainWindow::UiSnapshotApplied, qt_app_,
                     [this](const domain::PlayerSnapshot &snapshot) {
                         if (smoke_completed_) {
                             return;
                         }

                         if (snapshot.state == domain::PlaybackState::kPlaying) {
                             smoke_completed_ = true;
                             std::cout << "ShaTV Stage3 mpv smoke ok state=playing" << std::endl;
                             QTimer::singleShot(0, qt_app_, &QCoreApplication::quit);
                             return;
                         }

                         if (snapshot.state == domain::PlaybackState::kError) {
                             smoke_completed_ = true;
                             std::cout << "ShaTV Stage3 mpv smoke failed state=error" << std::endl;
                             QTimer::singleShot(0, qt_app_, &QCoreApplication::quit);
                         }
                     });

    QTimer::singleShot(0, main_window_.get(), &ui::windows::MainWindow::StartSmokeScenario);
    QTimer::singleShot(1500, qt_app_, [this]() {
        if (smoke_completed_) {
            return;
        }

        std::cout << "ShaTV Stage3 mpv smoke timeout" << std::endl;
        qt_app_->quit();
    });
}

}  // namespace shatv::app
