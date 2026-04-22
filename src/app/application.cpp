#include "app/application.h"

#include <iostream>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QWindow>

#include "app/epg_service.h"
#include "app/m3u_playlist_parser.h"
#include "app/xmltv_epg_payload.h"
#include "application/player_controller.h"
#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/fake_player_backend.h"
#include "player/mpv_player_backend.h"
#include "ui/models/channel_filter_model.h"
#include "ui/models/channel_list_model.h"
#include "ui/shell/app_shell_bridge.h"
#include "ui/video/mpv_video_item.h"

namespace shatv::app {

namespace {

domain::Channel BuildSmokeTestChannel() {
    return {
        .id = "smoke-test",
        .name = "Smoke Test",
        .url = QUrl("https://example.com/smoke-test.m3u8"),
        .group = "Smoke",
        .tvg_id = {},
        .tvg_name = {},
    };
}

RecentOpenItem BuildRecentFileItem(const QString &path, const QString &current_directory) {
    const domain::Channel channel = BuildOpenMediaChannel(path, current_directory);
    const QString target =
        channel.url.isLocalFile() ? QFileInfo(channel.url.toLocalFile()).absoluteFilePath() : channel.url.toString();
    return {
        .kind = "file",
        .target = target,
        .label = channel.name,
    };
}

RecentOpenItem BuildRecentUrlItem(const QString &url_text, const QString &current_directory) {
    const domain::Channel channel = BuildOpenUrlChannel(url_text, current_directory);
    return {
        .kind = "url",
        .target = channel.url.toString(),
        .label = channel.name,
    };
}

QString FormatProgrammeText(const std::optional<XmltvProgramme> &programme) {
    if (!programme.has_value()) {
        return {};
    }

    const QDateTime local_start = programme->start_at.toLocalTime();
    const QDateTime local_stop = programme->stop_at.toLocalTime();
    return QString("%1-%2 %3").arg(local_start.toString("HH:mm"), local_stop.toString("HH:mm"), programme->title);
}

}  // namespace

Application::Application(QGuiApplication *qt_app, LaunchOptions options)
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
    channel_filter_model_ = std::make_unique<ui::models::ChannelFilterModel>();
    channel_filter_model_->setSourceModel(channel_model_.get());
    shell_bridge_ = std::make_unique<ui::shell::AppShellBridge>(channel_filter_model_.get());
    qml_engine_ = std::make_unique<QQmlApplicationEngine>();
    network_manager_ = std::make_unique<QNetworkAccessManager>();

    status_message_timer_.setSingleShot(true);
    QObject::connect(&status_message_timer_, &QTimer::timeout, qt_app_, [this]() {
        status_message_.clear();
        shell_bridge_->SetStatusMessage(QString());
    });

    if (!settings_.Load()) {
        std::cerr << "ShaTV config load failed path=" << settings_.ConfigPath().toStdString() << std::endl;
    }

    controller_->SetVolume(settings_.Volume());
    controller_->SetMuted(settings_.Muted());

    qml_engine_->rootContext()->setContextProperty(QStringLiteral("appShellBridge"), shell_bridge_.get());
    ui::video::RegisterQmlVideoTypes();
    qml_engine_->load(QUrl(QStringLiteral("qrc:/qt/qml/MainWindow.qml")));
    if (qml_engine_->rootObjects().isEmpty()) {
        qFatal("ShaTV failed to load MainWindow.qml");
    }

    qml_root_object_ = qml_engine_->rootObjects().constFirst();
    root_window_ = qobject_cast<QWindow *>(qml_root_object_.data());
    Q_ASSERT(root_window_ != nullptr);

    video_item_ =
        qobject_cast<ui::video::MpvVideoItem *>(qml_root_object_->findChild<QObject *>(QStringLiteral("playerVideoItem")));
    Q_ASSERT(video_item_ != nullptr);

    if (auto *mpv_backend = dynamic_cast<player::MpvPlayerBackend *>(backend_.get())) {
        mpv_backend->SetNetworkUserAgent(settings_.UserAgent());
        video_item_->SetBackend(mpv_backend);
    }

    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::ActivateChannelRequested, qt_app_,
                     [this](const QModelIndex &index) {
                         const QModelIndex source_index = channel_filter_model_->mapToSource(index);
                         const domain::Channel channel = channel_model_->ChannelAt(source_index);
                         if (channel.id.isEmpty()) {
                             return;
                         }
                         controller_->PlayChannel(channel);
                     });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::PlayPauseRequested, qt_app_, [this]() {
        const domain::PlaybackState state = controller_->CurrentSnapshot().state;
        if (state == domain::PlaybackState::kPlaying || state == domain::PlaybackState::kLoading ||
            state == domain::PlaybackState::kBuffering || state == domain::PlaybackState::kRetrying) {
            controller_->Pause();
            return;
        }
        controller_->Resume();
    });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::StopRequested, controller_.get(),
                     &application::PlayerController::Stop);
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::MuteRequested, controller_.get(),
                     &application::PlayerController::SetMuted);
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::VolumeRequested, controller_.get(),
                     &application::PlayerController::SetVolume);
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::OpenFileRequested, qt_app_,
                     [this](const QString &path) { OpenFile(path); });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::OpenUrlRequested, qt_app_,
                     [this](const QString &url_text) { OpenUrl(url_text); });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::NetworkSettingsRequested, qt_app_,
                     [this](const QString &user_agent, const QString &epg_url) {
                         UpdateNetworkSettings(user_agent, epg_url);
                     });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::RecentOpenRequested, qt_app_,
                     [this](const QString &kind, const QString &target) { OpenRecentItem(kind, target); });

    QObject::connect(controller_.get(), &application::PlayerController::PlaybackSnapshotChanged, qt_app_,
                     [this](const domain::PlayerSnapshot &snapshot) {
                         shell_bridge_->SetPlaybackSnapshot(snapshot);
                         if (!snapshot.channel_id.isEmpty()) {
                             channel_model_->SetCurrentChannelId(snapshot.channel_id);
                         }
                         if (!snapshot.message.isEmpty()) {
                             ShowStatusMessage(snapshot.message, 3000);
                         }
                         if (snapshot.volume != settings_.Volume()) {
                             settings_.SetVolume(snapshot.volume);
                         }
                         if (snapshot.muted != settings_.Muted()) {
                             settings_.SetMuted(snapshot.muted);
                         }
                         UpdateDisplayedEpg();
                     });
    QObject::connect(controller_.get(), &application::PlayerController::CurrentChannelChanged, qt_app_,
                     [this](const QString &) { UpdateDisplayedEpg(); });

    epg_refresh_timer_.setInterval(60 * 1000);
    QObject::connect(&epg_refresh_timer_, &QTimer::timeout, qt_app_, [this]() { UpdateDisplayedEpg(); });
    epg_refresh_timer_.start();

    startup_channel_ = BuildStartupChannel(options_, qEnvironmentVariable("SHATV_SMOKE_MEDIA"), QDir::currentPath());
    initial_channels_ = BuildInitialChannels();
    current_channels_ = initial_channels_;
    channel_model_->SetChannels(initial_channels_);
    RefreshShellFilters();
    RefreshRecentItems();
    shell_bridge_->SetStatusMessage(QString());
    shell_bridge_->SetProgrammeTexts(QString(), QString());
    shell_bridge_->SetPlaybackSnapshot(controller_->CurrentSnapshot());
    shell_bridge_->SetConfiguredUserAgent(settings_.UserAgent());
    shell_bridge_->SetConfiguredEpgUrl(settings_.EpgUrl());
}

Application::~Application() {
    status_message_timer_.stop();
    if (video_item_ != nullptr) {
        video_item_->SetBackend(nullptr);
    }

    if (!settings_.Save()) {
        std::cerr << "ShaTV config save failed on exit" << std::endl;
    }

    qml_engine_.reset();
    controller_.reset();
    backend_.reset();
    shell_bridge_.reset();
    channel_filter_model_.reset();
    channel_model_.reset();
}

int Application::Run() {
    Q_ASSERT(root_window_ != nullptr);
    root_window_->show();

    if (options_.smoke_test) {
        std::cout << "ShaTV Qt6 bootstrap" << std::endl;
        SetupSmokeScenario();
    } else if (options_.mpv_smoke) {
        SetupMpvSmokeScenario();
    } else if (!options_.open_url_argument.isEmpty()) {
        const QString startup_url = options_.open_url_argument;
        QTimer::singleShot(0, qt_app_, [this, startup_url]() { OpenUrl(startup_url); });
    } else if (!options_.open_media_argument.isEmpty()) {
        const QString startup_media = options_.open_media_argument;
        QTimer::singleShot(0, qt_app_, [this, startup_media]() { OpenFile(startup_media); });
    }

    return qt_app_->exec();
}

std::vector<domain::Channel> Application::BuildInitialChannels() const {
    if (options_.mpv_smoke && startup_channel_.has_value()) {
        return {*startup_channel_};
    }

    if (options_.smoke_test) {
        return {BuildSmokeTestChannel()};
    }

    return {};
}

void Application::OpenChannel(const domain::Channel &channel) {
    if (!channel.url.isValid() || channel.url.toString().isEmpty()) {
        ShowAlert(QCoreApplication::translate("Application", "Open request failed: invalid media target."));
        return;
    }

    // 菜单入口统一替换成单项列表，避免当前阶段再引入复杂播放列表管理。
    OpenChannels({channel});
}

void Application::OpenChannels(std::vector<domain::Channel> channels, const QString &playlist_epg_url) {
    if (channels.empty()) {
        ShowPlaylistImportError(QCoreApplication::translate("Application", "Playlist contains no playable channels"));
        return;
    }

    current_channels_ = channels;
    playlist_epg_url_ = playlist_epg_url;
    channel_model_->SetChannels(std::move(channels));
    RefreshShellFilters();
    ReloadEpg();
    StartInitialPlayback();
}

void Application::OpenFile(const QString &path) {
    if (path.isEmpty()) {
        return;
    }

    if (LooksLikeLocalM3uPath(path)) {
        OpenPlaylistFile(path);
        return;
    }

    RememberRecentItem(BuildRecentFileItem(path, QDir::currentPath()));
    OpenChannel(BuildOpenMediaChannel(path, QDir::currentPath()));
}

void Application::OpenPlaylistFile(const QString &path) {
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ShowPlaylistImportError(QCoreApplication::translate("Application", "Failed to open playlist file"));
        return;
    }

    const QString text = QString::fromUtf8(input.readAll());
    const PlaylistImportResult playlist = ParsePlaylistImportText(text, QFileInfo(path).baseName());
    if (playlist.channels.empty()) {
        ShowPlaylistImportError(QCoreApplication::translate("Application", "Playlist contains no playable channels"));
        return;
    }

    RememberRecentItem(BuildRecentFileItem(path, QDir::currentPath()));
    OpenChannels(playlist.channels, playlist.epg_url);
}

void Application::OpenUrl(const QString &url_text) {
    if (url_text.isEmpty()) {
        return;
    }

    const domain::Channel channel = BuildOpenUrlChannel(url_text, QDir::currentPath());
    if (!IsRemotePlaybackUrl(channel.url)) {
        ShowAlert(QCoreApplication::translate("Application", "Open Link expects an http:// or https:// URL."));
        return;
    }
    if (LooksLikeRemoteM3uUrl(channel.url)) {
        RememberRecentItem(BuildRecentUrlItem(url_text, QDir::currentPath()));
        DownloadPlaylist(channel.url);
        return;
    }
    if (LooksLikeRemoteMediaDirectoryUrl(channel.url)) {
        ShowAlert(QCoreApplication::translate(
            "Application", "Open Link needs a full media URL, for example http://127.0.0.1:8080/index.m3u8"));
        return;
    }

    RememberRecentItem(BuildRecentUrlItem(url_text, QDir::currentPath()));
    OpenChannel(channel);
}

void Application::DownloadPlaylist(const QUrl &url) {
    QNetworkRequest request(url);
    if (!settings_.UserAgent().isEmpty()) {
        request.setHeader(QNetworkRequest::UserAgentHeader, settings_.UserAgent());
    }

    QNetworkReply *reply = network_manager_->get(request);
    QObject::connect(reply, &QNetworkReply::finished, qt_app_, [this, reply, url]() {
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

        const PlaylistImportResult playlist = ParsePlaylistImportText(text, QFileInfo(url.path()).baseName());
        if (playlist.channels.empty()) {
            ShowPlaylistImportError(QCoreApplication::translate("Application", "Playlist contains no playable channels"));
            return;
        }

        OpenChannels(playlist.channels, playlist.epg_url);
    });
}

void Application::ShowPlaylistImportError(const QString &message) {
    ShowAlert(message);
}

void Application::UpdateNetworkSettings(const QString &user_agent, const QString &epg_url) {
    const QString previous_user_agent = settings_.UserAgent();
    const QString previous_epg_url = settings_.EpgUrl();

    settings_.SetUserAgent(user_agent);
    settings_.SetEpgUrl(epg_url);
    if (!settings_.Save()) {
        settings_.SetUserAgent(previous_user_agent);
        settings_.SetEpgUrl(previous_epg_url);
        ShowAlert(QCoreApplication::translate("Application", "Failed to save network settings to %1")
                      .arg(QDir::toNativeSeparators(settings_.ConfigPath())));
        return;
    }

    if (auto *mpv_backend = dynamic_cast<player::MpvPlayerBackend *>(backend_.get())) {
        mpv_backend->SetNetworkUserAgent(settings_.UserAgent());
    }
    shell_bridge_->SetConfiguredUserAgent(settings_.UserAgent());
    shell_bridge_->SetConfiguredEpgUrl(settings_.EpgUrl());
    ReloadEpg();
    ShowStatusMessage(QCoreApplication::translate("Application", "Network settings saved"), 3000);
}

void Application::ShowAlert(const QString &message) {
    shell_bridge_->SetAlertMessage(message);
}

void Application::ReloadEpg() {
    ++epg_load_generation_;
    epg_service_ = EpgService{};
    ClearDisplayedEpg();

    const QString source_url = EpgService::ResolveSourceUrl(settings_.EpgUrl(), playlist_epg_url_);
    if (source_url.isEmpty() || current_channels_.empty()) {
        return;
    }

    const int generation = epg_load_generation_;
    const QUrl epg_url = QUrl::fromUserInput(source_url, QDir::currentPath(), QUrl::AssumeLocalFile);
    if (epg_url.isLocalFile()) {
        QFile input(epg_url.toLocalFile());
        if (!input.open(QIODevice::ReadOnly)) {
            std::cerr << "ShaTV EPG load failed source=" << source_url.toStdString() << " reason=read-local-file"
                      << std::endl;
            return;
        }

        QString decode_error;
        const std::optional<QString> xml = DecodeXmltvPayload(input.readAll(), source_url, &decode_error);
        if (!xml.has_value()) {
            std::cerr << "ShaTV EPG load failed source=" << source_url.toStdString()
                      << " reason=" << decode_error.toStdString() << std::endl;
            return;
        }

        EpgService loaded_service;
        QString parse_error;
        if (!loaded_service.LoadXmltv(*xml, &parse_error)) {
            std::cerr << "ShaTV EPG parse failed source=" << source_url.toStdString()
                      << " reason=" << parse_error.toStdString() << std::endl;
            return;
        }

        if (generation != epg_load_generation_) {
            return;
        }

        epg_service_ = std::move(loaded_service);
        UpdateDisplayedEpg();
        return;
    }

    QNetworkRequest request(epg_url);
    if (!settings_.UserAgent().isEmpty()) {
        request.setHeader(QNetworkRequest::UserAgentHeader, settings_.UserAgent());
    }

    QNetworkReply *reply = network_manager_->get(request);
    QObject::connect(reply, &QNetworkReply::finished, qt_app_, [this, reply, source_url, generation]() {
        const std::unique_ptr<QNetworkReply, void (*)(QNetworkReply *)> cleanup(reply, [](QNetworkReply *r) {
            if (r != nullptr) {
                r->deleteLater();
            }
        });

        if (generation != epg_load_generation_) {
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            std::cerr << "ShaTV EPG download failed source=" << source_url.toStdString()
                      << " reason=" << reply->errorString().toStdString() << std::endl;
            return;
        }

        QString decode_error;
        const std::optional<QString> xml = DecodeXmltvPayload(reply->readAll(), source_url, &decode_error);
        if (!xml.has_value()) {
            std::cerr << "ShaTV EPG load failed source=" << source_url.toStdString()
                      << " reason=" << decode_error.toStdString() << std::endl;
            return;
        }

        EpgService loaded_service;
        QString parse_error;
        if (!loaded_service.LoadXmltv(*xml, &parse_error)) {
            std::cerr << "ShaTV EPG parse failed source=" << source_url.toStdString()
                      << " reason=" << parse_error.toStdString() << std::endl;
            return;
        }

        if (generation != epg_load_generation_) {
            return;
        }

        epg_service_ = std::move(loaded_service);
        UpdateDisplayedEpg();
    });
}

void Application::UpdateDisplayedEpg() {
    const QString current_channel_id = controller_->CurrentSnapshot().channel_id;
    if (current_channel_id.isEmpty()) {
        ClearDisplayedEpg();
        return;
    }

    const std::optional<domain::Channel> channel = FindChannelById(current_channel_id);
    if (!channel.has_value()) {
        ClearDisplayedEpg();
        return;
    }

    const ChannelEpgNowNext now_next = epg_service_.LookupNowNext(*channel, QDateTime::currentDateTime());
    shell_bridge_->SetProgrammeTexts(FormatProgrammeText(now_next.current), FormatProgrammeText(now_next.next));
}

void Application::ClearDisplayedEpg() {
    shell_bridge_->SetProgrammeTexts(QString(), QString());
}

std::optional<domain::Channel> Application::FindChannelById(const QString &channel_id) const {
    for (const domain::Channel &channel : current_channels_) {
        if (channel.id == channel_id) {
            return channel;
        }
    }

    return std::nullopt;
}

void Application::RememberRecentItem(const RecentOpenItem &item) {
    if (options_.smoke_test || options_.mpv_smoke) {
        return;
    }

    settings_.RememberRecentItem(item);
    RefreshRecentItems();
    if (!settings_.Save()) {
        std::cerr << "ShaTV recent history save failed path=" << settings_.ConfigPath().toStdString() << std::endl;
        ShowStatusMessage(QCoreApplication::translate("Application", "Failed to save recent history"), 3000);
    }
}

void Application::RefreshRecentItems() {
    shell_bridge_->SetRecentItems(settings_.RecentItems());
}

void Application::RefreshShellFilters() {
    shell_bridge_->SetAvailableGroups(channel_filter_model_->AvailableGroups());
    shell_bridge_->SetCurrentGroupFilter(channel_filter_model_->GroupFilter());
    shell_bridge_->SetSearchTextValue(channel_filter_model_->SearchText());
}

void Application::ShowStatusMessage(const QString &message, int timeout_ms) {
    status_message_ = message;
    shell_bridge_->SetStatusMessage(status_message_);
    status_message_timer_.stop();

    if (timeout_ms <= 0 || message.isEmpty()) {
        return;
    }

    status_message_timer_.start(timeout_ms);
}

void Application::OpenRecentItem(const QString &kind, const QString &target) {
    if (kind == "file") {
        OpenFile(target);
        return;
    }
    if (kind == "url") {
        OpenUrl(target);
    }
}

void Application::StartInitialPlayback() {
    if (channel_filter_model_->rowCount() <= 0) {
        return;
    }

    const QModelIndex source_index = channel_filter_model_->mapToSource(channel_filter_model_->index(0, 0));
    const domain::Channel channel = channel_model_->ChannelAt(source_index);
    if (channel.id.isEmpty()) {
        return;
    }

    controller_->PlayChannel(channel);
}

void Application::SetupSmokeScenario() {
    QObject::connect(controller_.get(), &application::PlayerController::PlaybackSnapshotChanged, qt_app_,
                     [this](const domain::PlayerSnapshot &snapshot) {
                         if (smoke_completed_ || snapshot.state != domain::PlaybackState::kPlaying) {
                             return;
                         }

                         smoke_completed_ = true;
                         const QString state_name = domain::PlaybackStateToken(controller_->CurrentSnapshot().state);

                         std::cout << "ShaTV Stage2 smoke ok channels=" << channel_model_->rowCount()
                                   << " current=" << controller_->CurrentSnapshot().channel_id.toStdString()
                                   << " state=" << state_name.toStdString() << std::endl;

                         QTimer::singleShot(0, qt_app_, &QCoreApplication::quit);
                     });

    // 通过定时触发模拟一次真实的首频道点击，验证 UI -> Controller -> Backend -> UI 主链。
    QTimer::singleShot(0, qt_app_, [this]() { StartInitialPlayback(); });
}

void Application::SetupMpvSmokeScenario() {
    QObject::connect(controller_.get(), &application::PlayerController::PlaybackSnapshotChanged, qt_app_,
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

    QTimer::singleShot(0, qt_app_, [this]() { StartInitialPlayback(); });
    QTimer::singleShot(1500, qt_app_, [this]() {
        if (smoke_completed_) {
            return;
        }

        std::cout << "ShaTV Stage3 mpv smoke timeout" << std::endl;
        qt_app_->quit();
    });
}

}  // namespace shatv::app
