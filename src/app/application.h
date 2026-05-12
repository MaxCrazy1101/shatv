#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <QGuiApplication>
#include <QFutureWatcher>
#include <QPointer>
#include <QString>
#include <QTimer>

#include "app/app_settings.h"
#include "app/asr_model_archive_installer.h"
#include "app/asr_model_service.h"
#include "app/epg_service.h"
#include "app/launch_options.h"
#include "app/open_request.h"
#include "domain/media_source.h"

class QNetworkAccessManager;
class QObject;
class QQmlApplicationEngine;
class QWindow;

namespace shatv::application {
class PlayerController;
}

namespace shatv::player {
class PlayerBackend;
}

namespace shatv::ui::models {
class ChannelFilterModel;
class ChannelListModel;
}

namespace shatv::ui::shell {
class AppShellBridge;
}

namespace shatv::ui::video {
class VideoPresenterItem;
}

namespace shatv::app {

class SourceOpenService;

class Application final {
   public:
    Application(QGuiApplication *qt_app, LaunchOptions options);
    ~Application();
    int Run();

   private:
    std::vector<domain::ResolvedChannel> BuildInitialChannels() const;
    void ResolveOpenRequest(OpenRequest request);
    void HandleOpenResolution(OpenResolution resolution);
    void OpenChannels(std::vector<domain::ResolvedChannel> channels, const QString &playlist_epg_url = QString());
    void UpdateNetworkSettings(const QString &user_agent, const QString &epg_url);
    void ShowAlert(const QString &message);
    void ReloadEpg();
    void UpdateDisplayedEpg();
    void ClearDisplayedEpg();
    std::optional<domain::Channel> FindChannelById(const QString &channel_id) const;
    const domain::ResolvedChannel *FindResolvedChannelBySourceRow(int row) const;
    void RememberRecentItem(const RecentOpenItem &item);
    void RefreshRecentItems();
    void RefreshShellFilters();
    void ShowStatusMessage(const QString &message, int timeout_ms = 3000);
    void OpenRecentItem(const QString &request_kind, const QString &target);
    bool SpeechSubtitleAvailable(QString *unavailable_reason) const;
    void RefreshSpeechSubtitleControl();
    void UpdateSpeechSubtitleEnabled(bool enabled);
    void RefreshSpeechModelStatus();
    void DownloadSpeechModel();
    void CancelSpeechModelDownload();
    void UpdateSpeechModelDownloadProgress(qint64 bytes_received, qint64 bytes_total);
    void FinishSpeechModelDownload(const AsrModelArchiveDownloadResult &result);
    void InstallSpeechModelArchive(const QString &archive_path);
    void FinishSpeechModelArchiveInstall();
    void DeleteSpeechModel();
    void StartInitialPlayback();
    void SetupFfmpegAudioSmokeScenario();
    void SetupFfmpegSmokeScenario();
    void OpenLogsFolder();
    void CopyDiagnosticsToClipboard();
    QString BuildDiagnosticsText() const;

    QGuiApplication *qt_app_ = nullptr;
    LaunchOptions options_;
    AppSettings settings_;
    bool smoke_completed_ = false;
    std::unique_ptr<QNetworkAccessManager> network_manager_;
    std::unique_ptr<SourceOpenService> source_open_service_;
    std::unique_ptr<player::PlayerBackend> backend_;
    std::unique_ptr<application::PlayerController> controller_;
    std::unique_ptr<ui::models::ChannelListModel> channel_model_;
    std::unique_ptr<ui::models::ChannelFilterModel> channel_filter_model_;
    std::unique_ptr<ui::shell::AppShellBridge> shell_bridge_;
    std::unique_ptr<QQmlApplicationEngine> qml_engine_;
    QPointer<QObject> qml_root_object_;
    QPointer<QWindow> root_window_;
    QPointer<ui::video::VideoPresenterItem> ffmpeg_video_item_;
    EpgService epg_service_;
    std::vector<domain::ResolvedChannel> initial_channels_;
    std::vector<domain::ResolvedChannel> current_channels_;
    QString playlist_epg_url_;
    QString status_message_;
    QTimer epg_refresh_timer_;
    QTimer status_message_timer_;
    QFutureWatcher<AsrModelArchiveInstallResult> speech_model_install_watcher_;
    std::unique_ptr<AsrModelArchiveDownloader> speech_model_downloader_;
    int epg_load_generation_ = 0;
};

}  // namespace shatv::app
