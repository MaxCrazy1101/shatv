#include "app/application.h"

#include <iostream>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#include "app/build_info.h"
#include "app/epg_service.h"
#include "app/logging.h"
#include "app/source_open_service.h"
#include "app/xmltv_epg_payload.h"
#include "application/player_controller.h"
#include "domain/player_snapshot.h"
#include "domain/playback_state.h"
#include "player/ffmpeg_player_backend.h"
#include "ui/models/channel_filter_model.h"
#include "ui/models/channel_list_model.h"
#include "ui/shell/app_shell_bridge.h"
#include "ui/video/video_presenter_item.h"

namespace shatv::app {

namespace {

std::vector<domain::Channel> ExtractChannels(const std::vector<domain::ResolvedChannel> &resolved_channels) {
    std::vector<domain::Channel> channels;
    channels.reserve(resolved_channels.size());
    for (const domain::ResolvedChannel &resolved_channel : resolved_channels) {
        channels.push_back(resolved_channel.channel);
    }
    return channels;
}

QString FormatProgrammeText(const std::optional<XmltvProgramme> &programme) {
    if (!programme.has_value()) {
        return {};
    }

    const QDateTime local_start = programme->start_at.toLocalTime();
    const QDateTime local_stop = programme->stop_at.toLocalTime();
    return QString("%1-%2 %3").arg(local_start.toString("HH:mm"), local_stop.toString("HH:mm"), programme->title);
}

QString OpenTargetForLog(const OpenRequest &request) {
    if (request.request_kind == OpenRequestKind::kUrlText ||
        request.request_kind == OpenRequestKind::kStartupOpenUrl) {
        return RedactUrlForLog(QUrl::fromUserInput(request.target));
    }

    const QUrl url = QUrl::fromUserInput(request.target, QDir::currentPath(), QUrl::AssumeLocalFile);
    return RedactUrlForLog(url);
}

QString FfmpegAudioSmokeMediaPath() {
    return qEnvironmentVariable("SHATV_FFMPEG_AUDIO_SMOKE_MEDIA");
}

QString FfmpegSmokeMediaPath() {
    const QString env_path = qEnvironmentVariable("SHATV_FFMPEG_SMOKE_MEDIA");
    if (!env_path.isEmpty()) {
        return env_path;
    }

    const QString local_fixture = QDir(QDir::currentPath()).filePath("testdata/fixtures/ffmpeg_smoke_video.mp4");
    if (QFile::exists(local_fixture)) {
        return local_fixture;
    }

    const QString app_relative_fixture =
        QDir(QCoreApplication::applicationDirPath()).filePath("../../testdata/fixtures/ffmpeg_smoke_video.mp4");
    if (QFile::exists(app_relative_fixture)) {
        return QFileInfo(app_relative_fixture).absoluteFilePath();
    }

    return {};
}

}  // namespace

Application::Application(QGuiApplication *qt_app, LaunchOptions options)
    : qt_app_(qt_app), options_(std::move(options)), settings_(AppSettings::DefaultConfigPath()) {
    Q_ASSERT(qt_app_ != nullptr);

    qRegisterMetaType<domain::PlayerSnapshot>("shatv::domain::PlayerSnapshot");
    qRegisterMetaType<domain::MediaSourceDescriptor>("shatv::domain::MediaSourceDescriptor");
    qRegisterMetaType<domain::ResolvedChannel>("shatv::domain::ResolvedChannel");

    backend_ = std::make_unique<player::FfmpegPlayerBackend>();

    controller_ = std::make_unique<application::PlayerController>(backend_.get());
    channel_model_ = std::make_unique<ui::models::ChannelListModel>();
    channel_filter_model_ = std::make_unique<ui::models::ChannelFilterModel>();
    channel_filter_model_->setSourceModel(channel_model_.get());
    shell_bridge_ = std::make_unique<ui::shell::AppShellBridge>(channel_filter_model_.get());
    shell_bridge_->SetLogPaths(CurrentLogFilePath(), LogsDirectoryPath());
    qml_engine_ = std::make_unique<QQmlApplicationEngine>();
    QObject::connect(qml_engine_.get(), &QQmlApplicationEngine::warnings, qt_app_,
                     [](const QList<QQmlError> &warnings) {
                         for (const QQmlError &warning : warnings) {
                             qCWarning(log_qml).noquote() << warning.toString();
                         }
                     });
    network_manager_ = std::make_unique<QNetworkAccessManager>();
    source_open_service_ = std::make_unique<SourceOpenService>(network_manager_.get());

    status_message_timer_.setSingleShot(true);
    QObject::connect(&status_message_timer_, &QTimer::timeout, qt_app_, [this]() {
        status_message_.clear();
        shell_bridge_->SetStatusMessage(QString());
    });

    if (!settings_.Load()) {
        qCWarning(log_config).noquote()
            << "Config load failed path=" << QDir::toNativeSeparators(settings_.ConfigPath());
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

    ffmpeg_video_item_ = qobject_cast<ui::video::VideoPresenterItem *>(
        qml_root_object_->findChild<QObject *>(QStringLiteral("ffmpegVideoItem")));
    Q_ASSERT(ffmpeg_video_item_ != nullptr);

    if (auto *ffmpeg_backend = dynamic_cast<player::FfmpegPlayerBackend *>(backend_.get())) {
        ffmpeg_backend->SetVideoOnlyMode(options_.ffmpeg_smoke && qEnvironmentVariable("SHATV_FFMPEG_SMOKE_MEDIA").isEmpty());
        ffmpeg_video_item_->SetBackend(ffmpeg_backend);
    }

    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::ActivateChannelRequested, qt_app_,
                     [this](const QModelIndex &index) {
                         const QModelIndex source_index = channel_filter_model_->mapToSource(index);
                         const domain::ResolvedChannel *resolved_channel =
                             FindResolvedChannelBySourceRow(source_index.row());
                         if (resolved_channel == nullptr) {
                             return;
                         }
                         controller_->PlayResolvedChannel(*resolved_channel);
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
                     [this](const QString &path) {
                         ResolveOpenRequest(OpenRequest{
                             .request_kind = OpenRequestKind::kFilePath,
                             .target = path,
                             .label = {},
                             .replay_request_kind = std::nullopt,
                         });
                     });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::OpenUrlRequested, qt_app_,
                     [this](const QString &url_text) {
                         ResolveOpenRequest(OpenRequest{
                             .request_kind = OpenRequestKind::kUrlText,
                             .target = url_text,
                             .label = {},
                             .replay_request_kind = std::nullopt,
                         });
                     });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::NetworkSettingsRequested, qt_app_,
                     [this](const QString &user_agent, const QString &epg_url) {
                         UpdateNetworkSettings(user_agent, epg_url);
                     });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::OpenLogsFolderRequested, qt_app_,
                     [this]() { OpenLogsFolder(); });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::CopyDiagnosticsRequested, qt_app_,
                     [this]() { CopyDiagnosticsToClipboard(); });
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

    initial_channels_ = BuildInitialChannels();
    current_channels_ = initial_channels_;
    channel_model_->SetChannels(ExtractChannels(initial_channels_));
    RefreshShellFilters();
    RefreshRecentItems();
    shell_bridge_->SetStatusMessage(QString());
    shell_bridge_->SetProgrammeTexts(QString(), QString());
    shell_bridge_->SetPlaybackSnapshot(controller_->CurrentSnapshot());
    shell_bridge_->SetConfiguredUserAgent(settings_.UserAgent());
    shell_bridge_->SetConfiguredEpgUrl(settings_.EpgUrl());

    qCInfo(log_app).noquote()
        << "Application initialized"
        << "configPath=" << QDir::toNativeSeparators(settings_.ConfigPath())
        << "logFile=" << CurrentLogFilePath();
}

Application::~Application() {
    status_message_timer_.stop();
    if (ffmpeg_video_item_ != nullptr) {
        ffmpeg_video_item_->SetBackend(nullptr);
    }

    if (!options_.smoke_test && !options_.ffmpeg_audio_smoke && !options_.ffmpeg_smoke && !settings_.Save()) {
        qCWarning(log_config) << "Config save failed on exit";
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
        SetupFfmpegSmokeScenario();
    } else if (options_.ffmpeg_audio_smoke) {
        SetupFfmpegAudioSmokeScenario();
    } else if (options_.ffmpeg_smoke) {
        SetupFfmpegSmokeScenario();
    } else if (!options_.open_url_argument.isEmpty()) {
        const QString startup_url = options_.open_url_argument;
        QTimer::singleShot(0, qt_app_, [this, startup_url]() {
            ResolveOpenRequest(OpenRequest{
                .request_kind = OpenRequestKind::kStartupOpenUrl,
                .target = startup_url,
                .label = {},
                .replay_request_kind = std::nullopt,
            });
        });
    } else if (!options_.open_media_argument.isEmpty()) {
        const QString startup_media = options_.open_media_argument;
        QTimer::singleShot(0, qt_app_, [this, startup_media]() {
            ResolveOpenRequest(OpenRequest{
                .request_kind = OpenRequestKind::kStartupOpenMedia,
                .target = startup_media,
                .label = {},
                .replay_request_kind = std::nullopt,
            });
        });
    }

    const int exit_code = qt_app_->exec();
    qCInfo(log_app) << "Application event loop finished" << "exitCode=" << exit_code;
    return exit_code;
}

std::vector<domain::ResolvedChannel> Application::BuildInitialChannels() const {
    return {};
}

void Application::ResolveOpenRequest(OpenRequest request) {
    qCInfo(log_app).noquote()
        << "Open request"
        << "kind=" << OpenRequestKindToken(request.request_kind)
        << "target=" << OpenTargetForLog(request);
    source_open_service_->Resolve(std::move(request),
                                  SourceOpenContext{
                                      .current_directory = QDir::currentPath(),
                                      .user_agent = settings_.UserAgent(),
                                  },
                                  [this](OpenResolution resolution) { HandleOpenResolution(std::move(resolution)); });
}

void Application::HandleOpenResolution(OpenResolution resolution) {
    if (auto *error = std::get_if<OpenErrorResolution>(&resolution); error != nullptr) {
        qCWarning(log_app).noquote() << "Open request failed reason=" << error->message;
        ShowAlert(error->message);
        return;
    }

    if (auto *direct_media = std::get_if<DirectMediaResolution>(&resolution); direct_media != nullptr) {
        qCInfo(log_playback).noquote()
            << "Open resolved direct media"
            << "name=" << direct_media->item.channel.name
            << "target=" << RedactUrlForLog(direct_media->item.channel.url);
        if (direct_media->recent_item.has_value()) {
            RememberRecentItem(*direct_media->recent_item);
        }

        std::vector<domain::ResolvedChannel> channels;
        channels.push_back(std::move(direct_media->item));
        OpenChannels(std::move(channels));
        return;
    }

    if (auto *channel_list = std::get_if<ChannelListResolution>(&resolution); channel_list != nullptr) {
        qCInfo(log_playback)
            << "Open resolved playlist"
            << "channels=" << static_cast<int>(channel_list->channels.size())
            << "hasPlaylistEpg=" << !channel_list->playlist_epg_url.isEmpty();
        if (channel_list->recent_item.has_value()) {
            RememberRecentItem(*channel_list->recent_item);
        }

        OpenChannels(std::move(channel_list->channels), channel_list->playlist_epg_url);
    }
}

void Application::OpenChannels(std::vector<domain::ResolvedChannel> channels, const QString &playlist_epg_url) {
    if (channels.empty()) {
        ShowAlert(QCoreApplication::translate("Application", "Playlist contains no playable channels"));
        return;
    }

    current_channels_ = std::move(channels);
    playlist_epg_url_ = playlist_epg_url;
    qCInfo(log_playback)
        << "Opening channel list"
        << "channels=" << static_cast<int>(current_channels_.size())
        << "hasPlaylistEpg=" << !playlist_epg_url_.isEmpty();
    channel_model_->SetChannels(ExtractChannels(current_channels_));
    RefreshShellFilters();
    ReloadEpg();
    StartInitialPlayback();
}

void Application::UpdateNetworkSettings(const QString &user_agent, const QString &epg_url) {
    const QString previous_user_agent = settings_.UserAgent();
    const QString previous_epg_url = settings_.EpgUrl();

    settings_.SetUserAgent(user_agent);
    settings_.SetEpgUrl(epg_url);
    if (!settings_.Save()) {
        settings_.SetUserAgent(previous_user_agent);
        settings_.SetEpgUrl(previous_epg_url);
        qCWarning(log_config).noquote()
            << "Network settings save failed path=" << QDir::toNativeSeparators(settings_.ConfigPath());
        ShowAlert(QCoreApplication::translate("Application", "Failed to save network settings to %1")
                      .arg(QDir::toNativeSeparators(settings_.ConfigPath())));
        return;
    }

    shell_bridge_->SetConfiguredUserAgent(settings_.UserAgent());
    shell_bridge_->SetConfiguredEpgUrl(settings_.EpgUrl());
    qCInfo(log_config).noquote()
        << "Network settings saved"
        << "hasUserAgent=" << !settings_.UserAgent().isEmpty()
        << "epgUrl=" << (settings_.EpgUrl().isEmpty()
                              ? QStringLiteral("<empty>")
                              : RedactUrlForLog(QUrl::fromUserInput(settings_.EpgUrl())));
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
        qCInfo(log_epg) << "EPG reload skipped" << "hasSource=" << !source_url.isEmpty()
                        << "channels=" << static_cast<int>(current_channels_.size());
        return;
    }

    const int generation = epg_load_generation_;
    const QUrl epg_url = QUrl::fromUserInput(source_url, QDir::currentPath(), QUrl::AssumeLocalFile);
    qCInfo(log_epg).noquote() << "EPG load started source=" << RedactUrlForLog(epg_url);
    if (epg_url.isLocalFile()) {
        QFile input(epg_url.toLocalFile());
        if (!input.open(QIODevice::ReadOnly)) {
            qCWarning(log_epg).noquote()
                << "EPG local load failed source=" << RedactUrlForLog(epg_url)
                << "reason=" << input.errorString();
            return;
        }

        QString decode_error;
        const std::optional<QString> xml = DecodeXmltvPayload(input.readAll(), source_url, &decode_error);
        if (!xml.has_value()) {
            qCWarning(log_epg).noquote()
                << "EPG decode failed source=" << RedactUrlForLog(epg_url)
                << "reason=" << decode_error;
            return;
        }

        EpgService loaded_service;
        QString parse_error;
        if (!loaded_service.LoadXmltv(*xml, &parse_error)) {
            qCWarning(log_epg).noquote()
                << "EPG parse failed source=" << RedactUrlForLog(epg_url)
                << "reason=" << parse_error;
            return;
        }

        if (generation != epg_load_generation_) {
            return;
        }

        epg_service_ = std::move(loaded_service);
        qCInfo(log_epg).noquote() << "EPG local load completed source=" << RedactUrlForLog(epg_url);
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
            const QUrl failed_url = QUrl::fromUserInput(source_url);
            qCWarning(log_epg).noquote()
                << "EPG download failed source=" << RedactUrlForLog(failed_url)
                << "status=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                << "reason=" << reply->errorString();
            return;
        }

        QString decode_error;
        const std::optional<QString> xml = DecodeXmltvPayload(reply->readAll(), source_url, &decode_error);
        if (!xml.has_value()) {
            const QUrl failed_url = QUrl::fromUserInput(source_url);
            qCWarning(log_epg).noquote()
                << "EPG decode failed source=" << RedactUrlForLog(failed_url)
                << "reason=" << decode_error;
            return;
        }

        EpgService loaded_service;
        QString parse_error;
        if (!loaded_service.LoadXmltv(*xml, &parse_error)) {
            const QUrl failed_url = QUrl::fromUserInput(source_url);
            qCWarning(log_epg).noquote()
                << "EPG parse failed source=" << RedactUrlForLog(failed_url)
                << "reason=" << parse_error;
            return;
        }

        if (generation != epg_load_generation_) {
            return;
        }

        epg_service_ = std::move(loaded_service);
        qCInfo(log_epg).noquote() << "EPG remote load completed source=" << RedactUrlForLog(QUrl::fromUserInput(source_url));
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
    for (const domain::ResolvedChannel &resolved_channel : current_channels_) {
        if (resolved_channel.channel.id == channel_id) {
            return resolved_channel.channel;
        }
    }

    return std::nullopt;
}

const domain::ResolvedChannel *Application::FindResolvedChannelBySourceRow(int row) const {
    if (row < 0 || row >= static_cast<int>(current_channels_.size())) {
        return nullptr;
    }

    return &current_channels_.at(static_cast<std::size_t>(row));
}

void Application::RememberRecentItem(const RecentOpenItem &item) {
    if (options_.smoke_test || options_.ffmpeg_audio_smoke || options_.ffmpeg_smoke) {
        return;
    }

    settings_.RememberRecentItem(item);
    RefreshRecentItems();
    if (!settings_.Save()) {
        qCWarning(log_config).noquote()
            << "Recent history save failed path=" << QDir::toNativeSeparators(settings_.ConfigPath());
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

void Application::OpenRecentItem(const QString &request_kind, const QString &target) {
    const std::optional<OpenRequestKind> parsed_request_kind = LegacyOpenRequestKindFromToken(request_kind);
    if (!parsed_request_kind.has_value() || *parsed_request_kind == OpenRequestKind::kRecentItem) {
        ShowAlert(QCoreApplication::translate("Application", "Recent item cannot be reopened"));
        return;
    }

    ResolveOpenRequest(OpenRequest{
        .request_kind = OpenRequestKind::kRecentItem,
        .target = target,
        .label = {},
        .replay_request_kind = *parsed_request_kind,
    });
}

void Application::StartInitialPlayback() {
    if (channel_filter_model_->rowCount() <= 0) {
        return;
    }

    const QModelIndex source_index = channel_filter_model_->mapToSource(channel_filter_model_->index(0, 0));
    const domain::ResolvedChannel *resolved_channel = FindResolvedChannelBySourceRow(source_index.row());
    if (resolved_channel == nullptr) {
        return;
    }

    qCInfo(log_playback).noquote() << "Starting initial playback channel=" << resolved_channel->channel.name;
    controller_->PlayResolvedChannel(*resolved_channel);
}

void Application::SetupFfmpegAudioSmokeScenario() {
    QObject::connect(controller_.get(), &application::PlayerController::PlaybackSnapshotChanged, qt_app_,
                     [this](const domain::PlayerSnapshot &snapshot) {
                         if (smoke_completed_) {
                             return;
                         }

                         if (snapshot.state == domain::PlaybackState::kPlaying) {
                             smoke_completed_ = true;
                             std::cout << "ShaTV FFmpeg audio smoke ok state=playing" << std::endl;
                             // Keep the process alive briefly so the smoke path proves real QAudioSink playback,
                             // not just demux/decode success.
                             QTimer::singleShot(1500, qt_app_, &QCoreApplication::quit);
                             return;
                         }

                         if (snapshot.state == domain::PlaybackState::kError) {
                             smoke_completed_ = true;
                             std::cout << "ShaTV FFmpeg audio smoke failed state=error message="
                                       << snapshot.message.toStdString() << std::endl;
                             QTimer::singleShot(0, qt_app_, []() { QCoreApplication::exit(1); });
                         }
                     });

    const QString media_path = FfmpegAudioSmokeMediaPath();
    if (media_path.isEmpty()) {
        smoke_completed_ = true;
        std::cout << "ShaTV FFmpeg audio smoke failed missing fixture; set SHATV_FFMPEG_AUDIO_SMOKE_MEDIA" << std::endl;
        QTimer::singleShot(0, qt_app_, []() { QCoreApplication::exit(1); });
        return;
    }

    QTimer::singleShot(0, qt_app_, [this, media_path]() {
        ResolveOpenRequest(OpenRequest{
            .request_kind = OpenRequestKind::kStartupOpenMedia,
            .target = media_path,
            .label = {},
            .replay_request_kind = std::nullopt,
        });
    });
    QTimer::singleShot(10000, qt_app_, [this]() {
        if (smoke_completed_) {
            return;
        }

        smoke_completed_ = true;
        std::cout << "ShaTV FFmpeg audio smoke timeout" << std::endl;
        QCoreApplication::exit(1);
    });
}

void Application::SetupFfmpegSmokeScenario() {
    auto saw_playing = std::make_shared<bool>(false);
    QObject::connect(controller_.get(), &application::PlayerController::PlaybackSnapshotChanged, qt_app_,
                     [this, saw_playing](const domain::PlayerSnapshot &snapshot) {
                         if (smoke_completed_) {
                             return;
                         }

                         if (snapshot.state == domain::PlaybackState::kPlaying) {
                             *saw_playing = true;
                             std::cout << "ShaTV FFmpeg video smoke ok state=playing" << std::endl;
                             return;
                         }

                         if (snapshot.state == domain::PlaybackState::kIdle && *saw_playing) {
                             smoke_completed_ = true;
                             std::cout << "ShaTV FFmpeg video smoke ok state=idle" << std::endl;
                             QTimer::singleShot(0, qt_app_, &QCoreApplication::quit);
                             return;
                         }

                         if (snapshot.state == domain::PlaybackState::kError) {
                             smoke_completed_ = true;
                             std::cout << "ShaTV FFmpeg video smoke failed state=error message="
                                       << snapshot.message.toStdString() << std::endl;
                             QTimer::singleShot(0, qt_app_, []() { QCoreApplication::exit(1); });
                         }
                     });

    const QString media_path = FfmpegSmokeMediaPath();
    if (media_path.isEmpty()) {
        smoke_completed_ = true;
        std::cout << "ShaTV FFmpeg video smoke failed missing fixture; set SHATV_FFMPEG_SMOKE_MEDIA" << std::endl;
        QTimer::singleShot(0, qt_app_, []() { QCoreApplication::exit(1); });
        return;
    }

    QTimer::singleShot(0, qt_app_, [this, media_path]() {
        ResolveOpenRequest(OpenRequest{
            .request_kind = OpenRequestKind::kStartupOpenMedia,
            .target = media_path,
            .label = {},
            .replay_request_kind = std::nullopt,
        });
    });
    QTimer::singleShot(60000, qt_app_, [this]() {
        if (smoke_completed_) {
            return;
        }

        smoke_completed_ = true;
        std::cout << "ShaTV FFmpeg video smoke timeout" << std::endl;
        QCoreApplication::exit(1);
    });
}

void Application::OpenLogsFolder() {
    const QString logs_directory = LogsDirectoryPath();
    if (logs_directory.isEmpty()) {
        ShowAlert(QCoreApplication::translate("Application", "Log folder is not available"));
        return;
    }

    qCInfo(log_app).noquote() << "Opening logs folder path=" << QDir::toNativeSeparators(logs_directory);
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(logs_directory))) {
        ShowAlert(QCoreApplication::translate("Application", "Failed to open logs folder"));
    }
}

void Application::CopyDiagnosticsToClipboard() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard == nullptr) {
        ShowAlert(QCoreApplication::translate("Application", "Clipboard is not available"));
        return;
    }

    clipboard->setText(BuildDiagnosticsText());
    qCInfo(log_app) << "Diagnostics copied to clipboard";
    ShowStatusMessage(QCoreApplication::translate("Application", "Diagnostics copied to clipboard"), 3000);
}

QString Application::BuildDiagnosticsText() const {
    QString diagnostics;
    QTextStream stream(&diagnostics);
    stream << "ShaTV diagnostics\n";
    stream << "Version: " << QString::fromUtf8(kProjectVersion) << '\n';
    stream << "Build: " << QString::fromUtf8(kBuildId) << '\n';
    stream << "Qt: " << QString::fromLatin1(qVersion()) << '\n';
    stream << "Platform: " << QGuiApplication::platformName() << '\n';
    stream << "OS: " << QSysInfo::prettyProductName() << '\n';
    stream << "CPU: " << QSysInfo::currentCpuArchitecture() << '\n';
    stream << "Log file: " << QDir::toNativeSeparators(CurrentLogFilePath()) << '\n';
    stream << "Logs folder: " << QDir::toNativeSeparators(LogsDirectoryPath()) << '\n';
    stream << "File logging enabled: " << (LoggingEnabled() ? "yes" : "no") << '\n';
    stream << "Note: Logs may include local file paths that are useful for diagnosis.\n";
    return diagnostics;
}

}  // namespace shatv::app
