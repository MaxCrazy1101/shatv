#include "app/application.h"

#include <iostream>
#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include <QCoreApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
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
#include <QtConcurrent/QtConcurrentRun>

#include "app/asr_model_archive_installer.h"
#include "app/build_info.h"
#include "app/asr_model_service.h"
#include "app/epg_programme_presentation.h"
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

constexpr int kNetworkTransferTimeoutMillis = 30000;

std::vector<domain::Channel> ExtractChannels(const std::vector<domain::ResolvedChannel> &resolved_channels) {
    std::vector<domain::Channel> channels;
    channels.reserve(resolved_channels.size());
    for (const domain::ResolvedChannel &resolved_channel : resolved_channels) {
        channels.push_back(resolved_channel.channel);
    }
    return channels;
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

QString FormatBytes(qint64 bytes) {
    if (bytes <= 0) {
        return QCoreApplication::translate("Application", "Unknown");
    }

    constexpr double kUnit = 1024.0;
    double value = static_cast<double>(bytes);
    QStringList units{
        QCoreApplication::translate("Application", "B"),
        QCoreApplication::translate("Application", "KiB"),
        QCoreApplication::translate("Application", "MiB"),
        QCoreApplication::translate("Application", "GiB"),
    };
    int unit_index = 0;
    while (value >= kUnit && unit_index < units.size() - 1) {
        value /= kUnit;
        ++unit_index;
    }

    const int precision = unit_index == 0 ? 0 : 1;
    return QStringLiteral("%1 %2").arg(value, 0, 'f', precision).arg(units.at(unit_index));
}

QString SpeechModelStatusToken(AsrModelInstallStatus status) {
    switch (status) {
        case AsrModelInstallStatus::kNotInstalled:
            return QStringLiteral("not_installed");
        case AsrModelInstallStatus::kIncomplete:
            return QStringLiteral("incomplete");
        case AsrModelInstallStatus::kInstalled:
            return QStringLiteral("installed");
        case AsrModelInstallStatus::kDeveloperOverride:
            return QStringLiteral("developer_override");
    }
    return QStringLiteral("unknown");
}

QString SpeechModelStatusText(const AsrModelStatus &status) {
    switch (status.status) {
        case AsrModelInstallStatus::kNotInstalled:
            return QCoreApplication::translate("Application", "Not installed");
        case AsrModelInstallStatus::kIncomplete:
            return QCoreApplication::translate("Application", "Incomplete");
        case AsrModelInstallStatus::kInstalled:
            return QCoreApplication::translate("Application", "Installed");
        case AsrModelInstallStatus::kDeveloperOverride:
            return QCoreApplication::translate("Application", "Developer override");
    }
    return QCoreApplication::translate("Application", "Unknown");
}

bool SpeechRuntimeAvailable() {
#if defined(SHATV_ENABLE_ASR)
    return true;
#else
    return false;
#endif
}

QString SpeechModelStatusDetail(const AsrModelStatus &status, bool install_supported) {
    if (!SpeechRuntimeAvailable()) {
        return QCoreApplication::translate("Application", "This build does not include the speech recognition runtime");
    }
    if (!install_supported) {
        return QCoreApplication::translate("Application", "Model archive extraction requires libarchive support");
    }
    if (!status.missing_files.isEmpty()) {
        return QCoreApplication::translate("Application", "Missing required file: %1")
            .arg(QDir::toNativeSeparators(status.missing_files.first()));
    }
    if (!status.message.isEmpty()) {
        return status.message;
    }
    if (!status.model_dir.isEmpty()) {
        return QDir::toNativeSeparators(status.model_dir);
    }
    return {};
}

#if defined(SHATV_ENABLE_ASR)
bool ValidateSpeechSubtitleProvider(QString *unavailable_reason) {
    const QString provider = qEnvironmentVariable("SHATV_ASR_PROVIDER").trimmed();
    if (provider.isEmpty() ||
        provider == QStringLiteral("cpu") ||
        provider == QStringLiteral("cuda") ||
        provider == QStringLiteral("coreml")) {
        return true;
    }

    if (unavailable_reason != nullptr) {
        *unavailable_reason =
            QCoreApplication::translate("Application", "Unsupported ASR provider: %1").arg(provider);
    }
    return false;
}
#endif

}  // namespace

Application::Application(QGuiApplication *qt_app, LaunchOptions options)
    : qt_app_(qt_app), options_(std::move(options)), settings_(AppSettings::DefaultConfigPath()) {
    Q_ASSERT(qt_app_ != nullptr);

    qRegisterMetaType<domain::PlayerSnapshot>("shatv::domain::PlayerSnapshot");
    qRegisterMetaType<domain::MediaSourceDescriptor>("shatv::domain::MediaSourceDescriptor");
    qRegisterMetaType<domain::ResolvedChannel>("shatv::domain::ResolvedChannel");
    qRegisterMetaType<app::AsrModelArchiveInstallResult>("shatv::app::AsrModelArchiveInstallResult");
    qRegisterMetaType<app::AsrModelArchiveDownloadResult>("shatv::app::AsrModelArchiveDownloadResult");

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
    QObject::connect(&speech_model_install_watcher_, &QFutureWatcher<AsrModelArchiveInstallResult>::finished,
                     qt_app_, [this]() { FinishSpeechModelArchiveInstall(); });

    if (!settings_.Load()) {
        qCWarning(log_config).noquote()
            << "Config load failed path=" << QDir::toNativeSeparators(settings_.ConfigPath());
    }

    controller_->SetVolume(settings_.Volume());
    controller_->SetMuted(settings_.Muted());
    RefreshSpeechModelStatus();
    RefreshSpeechSubtitleControl();

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
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::SpeechSubtitleEnabledRequested, qt_app_,
                     [this](bool enabled) { UpdateSpeechSubtitleEnabled(enabled); });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::SpeechModelStatusRefreshRequested, qt_app_,
                     [this]() { RefreshSpeechModelStatus(); });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::SpeechModelDownloadRequested, qt_app_,
                     [this]() { DownloadSpeechModel(); });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::SpeechModelDownloadCancelRequested, qt_app_,
                     [this]() { CancelSpeechModelDownload(); });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::SpeechModelArchiveInstallRequested, qt_app_,
                     [this](const QString &archive_path) { InstallSpeechModelArchive(archive_path); });
    QObject::connect(shell_bridge_.get(), &ui::shell::AppShellBridge::SpeechModelDeleteRequested, qt_app_,
                     [this]() { DeleteSpeechModel(); });
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
    QObject::connect(controller_.get(), &application::PlayerController::SpeechSubtitleChanged, qt_app_,
                     [this](const QString &text, bool is_final, qint64 latency_ms) {
                         shell_bridge_->SetSpeechSubtitle(text, is_final, latency_ms);
                     });
    QObject::connect(controller_.get(), &application::PlayerController::SpeechSubtitleCleared, qt_app_,
                     [this]() { shell_bridge_->ClearSpeechSubtitle(); });
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
    shell_bridge_->SetProgrammePresentation(EpgProgrammePresentation{});
    shell_bridge_->SetPlaybackSnapshot(controller_->CurrentSnapshot());
    shell_bridge_->ClearSpeechSubtitle();
    shell_bridge_->SetConfiguredUserAgent(settings_.UserAgent());
    shell_bridge_->SetConfiguredEpgUrl(settings_.EpgUrl());

    qCInfo(log_app).noquote()
        << "Application initialized"
        << "configPath=" << QDir::toNativeSeparators(settings_.ConfigPath())
        << "logFile=" << CurrentLogFilePath();
}

Application::~Application() {
    status_message_timer_.stop();
    if (speech_model_downloader_ != nullptr) {
        speech_model_downloader_->Cancel();
        speech_model_downloader_.reset();
    }
    if (speech_model_install_watcher_.isRunning()) {
        speech_model_install_watcher_.waitForFinished();
    }
    if (ffmpeg_video_item_ != nullptr) {
        ffmpeg_video_item_->SetBackend(nullptr);
    }

    if (!options_.ffmpeg_audio_smoke && !options_.ffmpeg_smoke && !settings_.Save()) {
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

    if (options_.ffmpeg_audio_smoke) {
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
    request.setTransferTimeout(kNetworkTransferTimeoutMillis);
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

    const QDateTime now = QDateTime::currentDateTime();
    const ChannelEpgNowNext now_next = epg_service_.LookupNowNext(*channel, now);
    shell_bridge_->SetProgrammePresentation(BuildEpgProgrammePresentation(now_next, now));
}

void Application::ClearDisplayedEpg() {
    shell_bridge_->SetProgrammePresentation(EpgProgrammePresentation{});
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
    if (options_.ffmpeg_audio_smoke || options_.ffmpeg_smoke) {
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

bool Application::SpeechSubtitleAvailable(QString *unavailable_reason) const {
#if defined(SHATV_ENABLE_ASR)
    const AsrModelService model_service;
    const AsrModelStatus model_status = model_service.InstalledModelStatus();
    if (!model_status.Available()) {
        if (unavailable_reason != nullptr) {
            if (model_status.status == AsrModelInstallStatus::kNotInstalled) {
                *unavailable_reason =
                    QCoreApplication::translate("Application", "Speech recognition model is not installed");
            } else if (!model_status.missing_files.isEmpty()) {
                *unavailable_reason =
                    QCoreApplication::translate("Application", "Required ASR model file is missing: %1")
                        .arg(model_status.missing_files.join(QStringLiteral(", ")));
            } else {
                *unavailable_reason = model_status.message;
            }
        }
        return false;
    }
    if (!ValidateSpeechSubtitleProvider(unavailable_reason)) {
        return false;
    }
    if (unavailable_reason != nullptr) {
        unavailable_reason->clear();
    }
    return true;
#else
    if (unavailable_reason != nullptr) {
        *unavailable_reason =
            QCoreApplication::translate("Application", "This build does not include speech recognition subtitles");
    }
    return false;
#endif
}

void Application::RefreshSpeechSubtitleControl() {
    QString unavailable_reason;
    const bool available = SpeechSubtitleAvailable(&unavailable_reason);
    const bool effective_enabled = settings_.SpeechSubtitleEnabled() && available;

    shell_bridge_->SetSpeechSubtitleControlState(effective_enabled, available, unavailable_reason);
    controller_->SetSpeechSubtitleEnabled(effective_enabled);
    if (!effective_enabled) {
        shell_bridge_->ClearSpeechSubtitle();
    }
}

void Application::UpdateSpeechSubtitleEnabled(bool enabled) {
    QString unavailable_reason;
    const bool available = SpeechSubtitleAvailable(&unavailable_reason);
    if (enabled && !available) {
        shell_bridge_->SetSpeechSubtitleControlState(false, false, unavailable_reason);
        controller_->SetSpeechSubtitleEnabled(false);
        shell_bridge_->ClearSpeechSubtitle();
        ShowStatusMessage(unavailable_reason, 3000);
        RefreshSpeechModelStatus();
        if (shell_bridge_->SpeechModelStatusToken() == QStringLiteral("not_installed") ||
            shell_bridge_->SpeechModelStatusToken() == QStringLiteral("incomplete")) {
            ShowAlert(QCoreApplication::translate(
                "Application",
                "Speech recognition subtitles require an installed model. Open Settings > Speech to install one."));
        }
        return;
    }

    const bool previous_enabled = settings_.SpeechSubtitleEnabled();
    settings_.SetSpeechSubtitleEnabled(enabled);
    if (!options_.ffmpeg_audio_smoke && !options_.ffmpeg_smoke && !settings_.Save()) {
        settings_.SetSpeechSubtitleEnabled(previous_enabled);
        qCWarning(log_config).noquote()
            << "Speech subtitle preference save failed path="
            << QDir::toNativeSeparators(settings_.ConfigPath());
        RefreshSpeechSubtitleControl();
        ShowAlert(QCoreApplication::translate("Application", "Failed to save speech subtitle setting"));
        return;
    }

    const bool effective_enabled = enabled && available;
    shell_bridge_->SetSpeechSubtitleControlState(effective_enabled, available, unavailable_reason);
    controller_->SetSpeechSubtitleEnabled(effective_enabled);
    if (!effective_enabled) {
        shell_bridge_->ClearSpeechSubtitle();
    }

    qCInfo(log_config)
        << "Speech subtitle preference saved"
        << "enabled=" << enabled
        << "effectiveEnabled=" << effective_enabled;
    ShowStatusMessage(enabled
                          ? QCoreApplication::translate("Application", "Speech recognition subtitles enabled")
                          : QCoreApplication::translate("Application", "Speech recognition subtitles disabled"),
                      3000);
}

void Application::RefreshSpeechModelStatus() {
    const AsrModelService model_service;
    const AsrModelManifest &manifest = model_service.SelectedManifest();
    const AsrModelStatus model_status = model_service.InstalledModelStatus();
    const bool runtime_available = SpeechRuntimeAvailable();
    const bool install_supported = AsrModelArchiveInstaller::Supported();
    const bool installed = model_status.Available();
    const bool developer_override = model_status.source == AsrModelInstallSource::kDeveloperOverride;

    shell_bridge_->SetSpeechModelStatus(SpeechModelStatusToken(model_status.status),
                                        SpeechModelStatusText(model_status),
                                        SpeechModelStatusDetail(model_status, install_supported),
                                        manifest.display_name,
                                        manifest.version,
                                        manifest.source_url,
                                        FormatBytes(manifest.archive_size_bytes),
                                        FormatBytes(manifest.installed_size_bytes),
                                        manifest.archive_sha256,
                                        manifest.license,
                                        manifest.attribution,
                                        QDir::toNativeSeparators(model_status.model_dir),
                                        installed,
                                        developer_override,
                                        runtime_available,
                                        install_supported);
}

void Application::DownloadSpeechModel() {
    if (speech_model_downloader_ != nullptr) {
        ShowStatusMessage(QCoreApplication::translate("Application", "ASR model download is already running"), 3000);
        return;
    }
    if (speech_model_install_watcher_.isRunning()) {
        ShowStatusMessage(QCoreApplication::translate("Application", "ASR model installation is already running"), 3000);
        return;
    }
    if (!SpeechRuntimeAvailable()) {
        ShowAlert(QCoreApplication::translate("Application", "This build does not include the speech recognition runtime"));
        return;
    }
    if (!AsrModelArchiveInstaller::Supported()) {
        ShowAlert(QCoreApplication::translate("Application", "ASR model archive extraction requires libarchive support"));
        return;
    }

    const AsrModelManifest manifest = AsrModelService::DefaultManifest();
    speech_model_downloader_ = std::make_unique<AsrModelArchiveDownloader>(network_manager_.get());
    QObject::connect(speech_model_downloader_.get(), &AsrModelArchiveDownloader::ProgressChanged, qt_app_,
                     [this](qint64 bytes_received, qint64 bytes_total) {
                         UpdateSpeechModelDownloadProgress(bytes_received, bytes_total);
                     });
    QObject::connect(speech_model_downloader_.get(), &AsrModelArchiveDownloader::Finished, qt_app_,
                     [this](const AsrModelArchiveDownloadResult &result) {
                         QTimer::singleShot(0, qt_app_, [this, result]() { FinishSpeechModelDownload(result); });
                     });

    shell_bridge_->SetSpeechModelBusy(true);
    shell_bridge_->SetSpeechModelOperation(true,
                                           -1.0,
                                           QCoreApplication::translate("Application", "Downloading ASR model..."));
    ShowStatusMessage(QCoreApplication::translate("Application", "Downloading ASR model..."), 0);
    qCInfo(log_app).noquote()
        << "ASR model download requested"
        << "source=" << manifest.source_url;
    speech_model_downloader_->Start(manifest);
}

void Application::CancelSpeechModelDownload() {
    if (speech_model_downloader_ == nullptr) {
        return;
    }

    shell_bridge_->SetSpeechModelOperation(true,
                                           -1.0,
                                           QCoreApplication::translate("Application", "Cancelling ASR model download..."));
    speech_model_downloader_->Cancel();
}

void Application::UpdateSpeechModelDownloadProgress(qint64 bytes_received, qint64 bytes_total) {
    const double progress = bytes_total > 0
                                ? std::clamp(static_cast<double>(bytes_received) / static_cast<double>(bytes_total),
                                             0.0,
                                             1.0)
                                : -1.0;
    const QString operation_text = bytes_total > 0
                                       ? QCoreApplication::translate("Application", "Downloading ASR model... %1 / %2")
                                             .arg(FormatBytes(bytes_received), FormatBytes(bytes_total))
                                       : QCoreApplication::translate("Application", "Downloading ASR model... %1")
                                             .arg(FormatBytes(bytes_received));
    shell_bridge_->SetSpeechModelOperation(true, progress, operation_text);
}

void Application::FinishSpeechModelDownload(const AsrModelArchiveDownloadResult &result) {
    speech_model_downloader_.reset();

    if (!result.success) {
        shell_bridge_->SetSpeechModelBusy(false);
        shell_bridge_->SetSpeechModelOperation(false, -1.0, QString());
        RefreshSpeechModelStatus();
        RefreshSpeechSubtitleControl();

        qCWarning(log_app).noquote()
            << "ASR model download failed"
            << "reason=" << result.error_message;
        if (result.error_message.contains(QStringLiteral("cancelled"), Qt::CaseInsensitive)) {
            ShowStatusMessage(QCoreApplication::translate("Application", "ASR model download cancelled"), 3000);
            return;
        }
        ShowAlert(result.error_message);
        return;
    }

    qCInfo(log_app).noquote()
        << "ASR model download finished"
        << "archive=" << QDir::toNativeSeparators(result.archive_path);
    shell_bridge_->SetSpeechModelBusy(false);
    shell_bridge_->SetSpeechModelOperation(false, -1.0, QString());
    InstallSpeechModelArchive(result.archive_path);
}

void Application::InstallSpeechModelArchive(const QString &archive_path) {
    if (speech_model_downloader_ != nullptr) {
        ShowStatusMessage(QCoreApplication::translate("Application", "ASR model download is still running"), 3000);
        return;
    }
    if (speech_model_install_watcher_.isRunning()) {
        ShowStatusMessage(QCoreApplication::translate("Application", "ASR model installation is already running"), 3000);
        return;
    }
    if (!SpeechRuntimeAvailable()) {
        ShowAlert(QCoreApplication::translate("Application", "This build does not include the speech recognition runtime"));
        return;
    }
    if (!AsrModelArchiveInstaller::Supported()) {
        ShowAlert(QCoreApplication::translate("Application", "ASR model archive extraction requires libarchive support"));
        return;
    }
    if (!QFileInfo(archive_path).isFile()) {
        ShowAlert(QCoreApplication::translate("Application", "ASR model archive is missing: %1")
                      .arg(QDir::toNativeSeparators(archive_path)));
        return;
    }

    const AsrModelManifest manifest = AsrModelService::DefaultManifest();
    const QString normalized_archive_path = QFileInfo(archive_path).absoluteFilePath();
    shell_bridge_->SetSpeechModelBusy(true);
    shell_bridge_->SetSpeechModelOperation(false,
                                           -1.0,
                                           QCoreApplication::translate("Application", "Installing ASR model..."));
    ShowStatusMessage(QCoreApplication::translate("Application", "Installing ASR model..."), 0);
    qCInfo(log_app).noquote()
        << "ASR model archive install requested"
        << "archive=" << QDir::toNativeSeparators(normalized_archive_path);

    speech_model_install_watcher_.setFuture(QtConcurrent::run([normalized_archive_path, manifest]() {
        const AsrModelArchiveInstaller installer;
        return installer.InstallVerifiedArchive(normalized_archive_path, manifest);
    }));
}

void Application::FinishSpeechModelArchiveInstall() {
    const AsrModelArchiveInstallResult result = speech_model_install_watcher_.result();
    shell_bridge_->SetSpeechModelBusy(false);
    shell_bridge_->SetSpeechModelOperation(false, -1.0, QString());
    RefreshSpeechModelStatus();
    RefreshSpeechSubtitleControl();

    if (!result.success) {
        qCWarning(log_app).noquote()
            << "ASR model archive install failed"
            << "reason=" << result.error_message;
        ShowAlert(result.error_message);
        return;
    }

    qCInfo(log_app).noquote()
        << "ASR model archive installed"
        << "installDir=" << QDir::toNativeSeparators(result.install_dir);
    ShowStatusMessage(QCoreApplication::translate("Application", "ASR model installed"), 3000);
}

void Application::DeleteSpeechModel() {
    if (speech_model_downloader_ != nullptr) {
        ShowStatusMessage(QCoreApplication::translate("Application", "ASR model download is still running"), 3000);
        return;
    }
    if (speech_model_install_watcher_.isRunning()) {
        ShowStatusMessage(QCoreApplication::translate("Application", "ASR model installation is still running"), 3000);
        return;
    }

    const AsrModelService model_service;
    const AsrModelStatus model_status = model_service.InstalledModelStatus();
    if (model_status.source == AsrModelInstallSource::kDeveloperOverride) {
        ShowAlert(QCoreApplication::translate("Application", "Developer override model directories are not managed by ShaTV"));
        return;
    }

    const QString install_dir = model_service.InstallDirectory();
    if (install_dir.isEmpty() || !QFileInfo(install_dir).exists()) {
        RefreshSpeechModelStatus();
        ShowStatusMessage(QCoreApplication::translate("Application", "ASR model is not installed"), 3000);
        return;
    }
    if (!QFileInfo(install_dir).isDir()) {
        ShowAlert(QCoreApplication::translate("Application", "ASR model install path is not a directory: %1")
                      .arg(QDir::toNativeSeparators(install_dir)));
        return;
    }
    if (!QDir(install_dir).removeRecursively()) {
        ShowAlert(QCoreApplication::translate("Application", "Failed to delete ASR model: %1")
                      .arg(QDir::toNativeSeparators(install_dir)));
        return;
    }

    qCInfo(log_app).noquote()
        << "ASR model deleted"
        << "installDir=" << QDir::toNativeSeparators(install_dir);
    RefreshSpeechModelStatus();
    RefreshSpeechSubtitleControl();
    ShowStatusMessage(QCoreApplication::translate("Application", "ASR model deleted"), 3000);
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
