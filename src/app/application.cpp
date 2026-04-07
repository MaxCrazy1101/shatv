#include "app/application.h"

#include <iostream>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStatusBar>
#include <QTimer>

#include "app/m3u_playlist_parser.h"
#include "application/player_controller.h"
#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/fake_player_backend.h"
#include "player/mpv_player_backend.h"
#include "player/mpv_render_widget.h"
#include "ui/models/channel_list_model.h"
#include "ui/windows/main_window.h"

namespace shatv::app {

namespace {

domain::Channel BuildSmokeTestChannel() {
    return {
        .id = "smoke-test",
        .name = "Smoke Test",
        .url = QUrl("https://example.com/smoke-test.m3u8"),
        .group = "Smoke",
    };
}

}  // namespace

Application::Application(QApplication *qt_app, LaunchOptions options)
    : qt_app_(qt_app), options_(std::move(options)), settings_(AppSettings::DefaultConfigPath()) {
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
    network_manager_ = std::make_unique<QNetworkAccessManager>();

    if (!settings_.Load()) {
        std::cerr << "ShaTV config load failed path=" << settings_.ConfigPath().toStdString() << std::endl;
    }
    main_window_->SetConfiguredUserAgent(settings_.UserAgent());

    if (auto *mpv_backend = dynamic_cast<player::MpvPlayerBackend *>(backend_.get())) {
        mpv_backend->SetNetworkUserAgent(settings_.UserAgent());
        mpv_backend->AttachRenderWidget(main_window_->RenderWidget());
        main_window_->RenderWidget()->SetBackend(mpv_backend);
    }

    QObject::connect(main_window_.get(), &ui::windows::MainWindow::OpenFileSelected, qt_app_,
                     [this](const QString &path) { OpenFile(path); });
    QObject::connect(main_window_.get(), &ui::windows::MainWindow::OpenUrlSelected, qt_app_,
                     [this](const QString &url_text) { OpenUrl(url_text); });
    QObject::connect(main_window_.get(), &ui::windows::MainWindow::UserAgentChanged, qt_app_,
                     [this](const QString &user_agent) { UpdateNetworkUserAgent(user_agent); });

    startup_channel_ = BuildStartupChannel(options_, qEnvironmentVariable("SHATV_SMOKE_MEDIA"), QDir::currentPath());
    initial_channels_ = BuildInitialChannels();
    main_window_->SetChannels(initial_channels_);
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
        const domain::Channel startup_channel = *startup_channel_;
        if (LooksLikePlaylistChannel(startup_channel)) {
            QTimer::singleShot(0, qt_app_, [this, startup_channel]() {
                if (startup_channel.url.isLocalFile()) {
                    OpenPlaylistFile(startup_channel.url.toLocalFile());
                    return;
                }
                DownloadPlaylist(startup_channel.url);
            });
        } else {
            QTimer::singleShot(0, main_window_.get(), &ui::windows::MainWindow::StartInitialPlayback);
        }
    }

    return qt_app_->exec();
}

std::vector<domain::Channel> Application::BuildInitialChannels() const {
    if (startup_channel_.has_value()) {
        return {*startup_channel_};
    }

    if (options_.smoke_test) {
        return {BuildSmokeTestChannel()};
    }

    return {};
}

void Application::OpenChannel(const domain::Channel &channel) {
    if (!channel.url.isValid() || channel.url.toString().isEmpty()) {
        QMessageBox::warning(main_window_.get(), QCoreApplication::translate("Application", "ShaTV"),
                             QCoreApplication::translate("Application", "Open request failed: invalid media target."));
        return;
    }

    // 菜单入口统一替换成单项列表，避免当前阶段再引入复杂播放列表管理。
    main_window_->SetChannels({channel});
    main_window_->StartInitialPlayback();
}

void Application::OpenChannels(std::vector<domain::Channel> channels) {
    if (channels.empty()) {
        ShowPlaylistImportError(QCoreApplication::translate("Application", "Playlist contains no playable channels"));
        return;
    }

    main_window_->SetChannels(std::move(channels));
    main_window_->StartInitialPlayback();
}

void Application::OpenFile(const QString &path) {
    if (path.isEmpty()) {
        return;
    }

    if (LooksLikeLocalM3uPath(path)) {
        OpenPlaylistFile(path);
        return;
    }

    OpenChannel(BuildOpenMediaChannel(path, QDir::currentPath()));
}

void Application::OpenPlaylistFile(const QString &path) {
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ShowPlaylistImportError(QCoreApplication::translate("Application", "Failed to open playlist file"));
        return;
    }

    const QString text = QString::fromUtf8(input.readAll());
    const auto channels = ParseM3uPlaylistText(text, QFileInfo(path).baseName());
    if (channels.empty()) {
        ShowPlaylistImportError(QCoreApplication::translate("Application", "Playlist contains no playable channels"));
        return;
    }

    OpenChannels(channels);
}

void Application::OpenUrl(const QString &url_text) {
    if (url_text.isEmpty()) {
        return;
    }

    const domain::Channel channel = BuildOpenUrlChannel(url_text, QDir::currentPath());
    if (!IsRemotePlaybackUrl(channel.url)) {
        QMessageBox::warning(
            main_window_.get(), QCoreApplication::translate("Application", "ShaTV"),
            QCoreApplication::translate("Application", "Open Link expects an http:// or https:// URL."));
        return;
    }
    if (LooksLikeRemoteM3uUrl(channel.url)) {
        DownloadPlaylist(channel.url);
        return;
    }
    if (LooksLikeRemoteMediaDirectoryUrl(channel.url)) {
        QMessageBox::warning(
            main_window_.get(), QCoreApplication::translate("Application", "ShaTV"),
            QCoreApplication::translate(
                "Application",
                "Open Link needs a full media URL, for example http://127.0.0.1:8080/index.m3u8"));
        return;
    }

    OpenChannel(channel);
}

void Application::DownloadPlaylist(const QUrl &url) {
    QNetworkRequest request(url);
    if (!settings_.UserAgent().isEmpty()) {
        request.setHeader(QNetworkRequest::UserAgentHeader, settings_.UserAgent());
    }

    QNetworkReply *reply = network_manager_->get(request);
    QObject::connect(reply, &QNetworkReply::finished, main_window_.get(), [this, reply, url]() {
        const std::unique_ptr<QNetworkReply, void (*)(QNetworkReply *)> cleanup(reply, [](QNetworkReply *r) {
            if (r != nullptr) {
                r->deleteLater();
            }
        });

        if (reply->error() != QNetworkReply::NoError) {
            ShowPlaylistImportError(QCoreApplication::translate("Application", "Failed to download playlist"));
            return;
        }

        const QString text = QString::fromUtf8(reply->readAll());
        if (!LooksLikeM3uPlaylistText(text)) {
            ShowPlaylistImportError(QCoreApplication::translate("Application", "Playlist format is not supported"));
            return;
        }

        const auto channels = ParseM3uPlaylistText(text, QFileInfo(url.path()).baseName());
        if (channels.empty()) {
            ShowPlaylistImportError(QCoreApplication::translate("Application", "Playlist contains no playable channels"));
            return;
        }

        OpenChannels(channels);
    });
}

void Application::ShowPlaylistImportError(const QString &message) {
    QMessageBox::warning(main_window_.get(), QCoreApplication::translate("Application", "ShaTV"), message);
}

void Application::UpdateNetworkUserAgent(const QString &user_agent) {
    settings_.SetUserAgent(user_agent);
    if (!settings_.Save()) {
        QMessageBox::warning(
            main_window_.get(), QCoreApplication::translate("Application", "ShaTV"),
            QCoreApplication::translate("Application", "Failed to save User-Agent to %1")
                .arg(QDir::toNativeSeparators(settings_.ConfigPath())));
        return;
    }

    main_window_->SetConfiguredUserAgent(settings_.UserAgent());
    if (auto *mpv_backend = dynamic_cast<player::MpvPlayerBackend *>(backend_.get())) {
        mpv_backend->SetNetworkUserAgent(settings_.UserAgent());
    }
    main_window_->statusBar()->showMessage(QCoreApplication::translate("Application", "Network settings saved"), 3000);
}

void Application::SetupSmokeScenario() {
    QObject::connect(main_window_.get(), &ui::windows::MainWindow::UiSnapshotApplied, qt_app_,
                     [this](const domain::PlayerSnapshot &snapshot) {
                         if (smoke_completed_ || snapshot.state != domain::PlaybackState::kPlaying) {
                             return;
                         }

                         smoke_completed_ = true;
                         const QString state_name = domain::PlaybackStateToken(main_window_->LastAppliedSnapshot().state);

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
