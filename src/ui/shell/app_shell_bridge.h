#pragma once

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QtGlobal>
#include <vector>

#include "app/app_settings.h"
#include "app/epg_programme_presentation.h"
#include "domain/player_snapshot.h"

namespace shatv::ui::models {
class ChannelFilterModel;
}

namespace shatv::ui::shell {

class AppShellBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *channelModel READ ChannelModel CONSTANT)
    Q_PROPERTY(QStringList availableGroups READ AvailableGroups NOTIFY AvailableGroupsChanged)
    Q_PROPERTY(QString currentGroupFilter READ CurrentGroupFilter NOTIFY CurrentGroupFilterChanged)
    Q_PROPERTY(QString searchText READ SearchText NOTIFY SearchTextChanged)
    Q_PROPERTY(QVariantList recentItems READ RecentItems NOTIFY RecentItemsChanged)
    Q_PROPERTY(QString statusMessage READ StatusMessage NOTIFY StatusMessageChanged)
    Q_PROPERTY(QString currentChannelName READ CurrentChannelName NOTIFY CurrentChannelNameChanged)
    Q_PROPERTY(bool hasProgrammeInfo READ HasProgrammeInfo NOTIFY HasProgrammeInfoChanged)
    Q_PROPERTY(QString currentProgrammeTitle READ CurrentProgrammeTitle NOTIFY CurrentProgrammeTitleChanged)
    Q_PROPERTY(QString currentProgrammeTimeText READ CurrentProgrammeTimeText NOTIFY CurrentProgrammeTimeTextChanged)
    Q_PROPERTY(double currentProgrammeProgress READ CurrentProgrammeProgress NOTIFY CurrentProgrammeProgressChanged)
    Q_PROPERTY(bool currentProgrammeProgressAvailable READ CurrentProgrammeProgressAvailable NOTIFY
                   CurrentProgrammeProgressAvailableChanged)
    Q_PROPERTY(QString nextProgrammeTitle READ NextProgrammeTitle NOTIFY NextProgrammeTitleChanged)
    Q_PROPERTY(QString nextProgrammeTimeText READ NextProgrammeTimeText NOTIFY NextProgrammeTimeTextChanged)
    Q_PROPERTY(QString playbackStateText READ PlaybackStateText NOTIFY PlaybackStateTextChanged)
    Q_PROPERTY(QString playbackStateToken READ PlaybackStateToken NOTIFY PlaybackStateTokenChanged)
    Q_PROPERTY(bool playing READ Playing NOTIFY PlayingChanged)
    Q_PROPERTY(bool muted READ Muted NOTIFY MutedChanged)
    Q_PROPERTY(int volume READ Volume NOTIFY VolumeChanged)
    Q_PROPERTY(QString configuredUserAgent READ ConfiguredUserAgent NOTIFY ConfiguredUserAgentChanged)
    Q_PROPERTY(QString configuredEpgUrl READ ConfiguredEpgUrl NOTIFY ConfiguredEpgUrlChanged)
    Q_PROPERTY(QString appVersion READ AppVersion CONSTANT)
    Q_PROPERTY(QString buildId READ BuildId CONSTANT)
    Q_PROPERTY(QString logFilePath READ LogFilePath NOTIFY LogFilePathChanged)
    Q_PROPERTY(QString logsDirectoryPath READ LogsDirectoryPath NOTIFY LogsDirectoryPathChanged)
    Q_PROPERTY(QString alertMessage READ AlertMessage NOTIFY AlertMessageChanged)
    Q_PROPERTY(bool alertVisible READ AlertVisible NOTIFY AlertVisibleChanged)
    Q_PROPERTY(QString speechSubtitleText READ SpeechSubtitleText NOTIFY SpeechSubtitleTextChanged)
    Q_PROPERTY(bool speechSubtitleEnabled READ SpeechSubtitleEnabled NOTIFY SpeechSubtitleEnabledChanged)
    Q_PROPERTY(bool speechSubtitleAvailable READ SpeechSubtitleAvailable NOTIFY SpeechSubtitleAvailableChanged)
    Q_PROPERTY(QString speechSubtitleUnavailableReason READ SpeechSubtitleUnavailableReason NOTIFY
                   SpeechSubtitleUnavailableReasonChanged)
    Q_PROPERTY(bool speechSubtitleActive READ SpeechSubtitleActive NOTIFY SpeechSubtitleActiveChanged)
    Q_PROPERTY(bool speechSubtitleFinal READ SpeechSubtitleFinal NOTIFY SpeechSubtitleFinalChanged)
    Q_PROPERTY(qint64 speechSubtitleLatencyMs READ SpeechSubtitleLatencyMs NOTIFY SpeechSubtitleLatencyMsChanged)
    Q_PROPERTY(QString speechSubtitleStatusText READ SpeechSubtitleStatusText NOTIFY SpeechSubtitleStatusTextChanged)
    Q_PROPERTY(QString speechModelStatusToken READ SpeechModelStatusToken NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelStatusText READ SpeechModelStatusText NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelStatusDetail READ SpeechModelStatusDetail NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelName READ SpeechModelName NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelVersion READ SpeechModelVersion NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelSourceUrl READ SpeechModelSourceUrl NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelArchiveSizeText READ SpeechModelArchiveSizeText NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelInstalledSizeText READ SpeechModelInstalledSizeText NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelChecksum READ SpeechModelChecksum NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelLicense READ SpeechModelLicense NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelAttribution READ SpeechModelAttribution NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(QString speechModelDirectory READ SpeechModelDirectory NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(bool speechModelInstalled READ SpeechModelInstalled NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(bool speechModelDeveloperOverride READ SpeechModelDeveloperOverride NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(bool speechModelRuntimeAvailable READ SpeechModelRuntimeAvailable NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(bool speechModelInstallSupported READ SpeechModelInstallSupported NOTIFY SpeechModelStatusChanged)
    Q_PROPERTY(bool speechModelBusy READ SpeechModelBusy NOTIFY SpeechModelBusyChanged)
    Q_PROPERTY(bool speechModelDownloadActive READ SpeechModelDownloadActive NOTIFY SpeechModelOperationChanged)
    Q_PROPERTY(double speechModelProgress READ SpeechModelProgress NOTIFY SpeechModelOperationChanged)
    Q_PROPERTY(QString speechModelOperationText READ SpeechModelOperationText NOTIFY SpeechModelOperationChanged)

   public:
    explicit AppShellBridge(ui::models::ChannelFilterModel *channel_model, QObject *parent = nullptr);

    QAbstractItemModel *ChannelModel() const;
    QStringList AvailableGroups() const;
    QString CurrentGroupFilter() const;
    QString SearchText() const;
    QVariantList RecentItems() const;
    QString StatusMessage() const;
    QString CurrentChannelName() const;
    bool HasProgrammeInfo() const;
    QString CurrentProgrammeTitle() const;
    QString CurrentProgrammeTimeText() const;
    double CurrentProgrammeProgress() const;
    bool CurrentProgrammeProgressAvailable() const;
    QString NextProgrammeTitle() const;
    QString NextProgrammeTimeText() const;
    QString PlaybackStateText() const;
    QString PlaybackStateToken() const;
    bool Playing() const;
    bool Muted() const;
    int Volume() const;
    QString ConfiguredUserAgent() const;
    QString ConfiguredEpgUrl() const;
    QString AppVersion() const;
    QString BuildId() const;
    QString LogFilePath() const;
    QString LogsDirectoryPath() const;
    QString AlertMessage() const;
    bool AlertVisible() const;
    QString SpeechSubtitleText() const;
    bool SpeechSubtitleEnabled() const;
    bool SpeechSubtitleAvailable() const;
    QString SpeechSubtitleUnavailableReason() const;
    bool SpeechSubtitleActive() const;
    bool SpeechSubtitleFinal() const;
    qint64 SpeechSubtitleLatencyMs() const;
    QString SpeechSubtitleStatusText() const;
    QString SpeechModelStatusToken() const;
    QString SpeechModelStatusText() const;
    QString SpeechModelStatusDetail() const;
    QString SpeechModelName() const;
    QString SpeechModelVersion() const;
    QString SpeechModelSourceUrl() const;
    QString SpeechModelArchiveSizeText() const;
    QString SpeechModelInstalledSizeText() const;
    QString SpeechModelChecksum() const;
    QString SpeechModelLicense() const;
    QString SpeechModelAttribution() const;
    QString SpeechModelDirectory() const;
    bool SpeechModelInstalled() const;
    bool SpeechModelDeveloperOverride() const;
    bool SpeechModelRuntimeAvailable() const;
    bool SpeechModelInstallSupported() const;
    bool SpeechModelBusy() const;
    bool SpeechModelDownloadActive() const;
    double SpeechModelProgress() const;
    QString SpeechModelOperationText() const;

    void SetAvailableGroups(QStringList groups);
    void SetCurrentGroupFilter(const QString &group);
    void SetSearchTextValue(const QString &search_text);
    void SetRecentItems(const std::vector<app::RecentOpenItem> &items);
    void SetStatusMessage(const QString &message);
    void SetProgrammePresentation(const app::EpgProgrammePresentation &presentation);
    void SetPlaybackSnapshot(const domain::PlayerSnapshot &snapshot);
    void SetConfiguredUserAgent(const QString &user_agent);
    void SetConfiguredEpgUrl(const QString &epg_url);
    void SetLogPaths(const QString &log_file_path, const QString &logs_directory_path);
    void SetAlertMessage(const QString &message);
    void SetSpeechSubtitleControlState(bool enabled, bool available, const QString &unavailable_reason);
    void SetSpeechSubtitle(const QString &text, bool is_final, qint64 latency_ms);
    void ClearSpeechSubtitle();
    void SetSpeechModelStatus(const QString &status_token, const QString &status_text, const QString &status_detail,
                              const QString &name, const QString &version, const QString &source_url,
                              const QString &archive_size_text, const QString &installed_size_text,
                              const QString &checksum, const QString &license, const QString &attribution,
                              const QString &directory, bool installed, bool developer_override, bool runtime_available,
                              bool install_supported);
    void SetSpeechModelBusy(bool busy);
    void SetSpeechModelOperation(bool download_active, double progress, const QString &operation_text);

    Q_INVOKABLE void activateChannelRow(int row);
    Q_INVOKABLE void setSearchText(const QString &search_text);
    Q_INVOKABLE void setGroupFilter(const QString &group);
    Q_INVOKABLE void requestPlayPause();
    Q_INVOKABLE void requestStop();
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void submitOpenFile(const QUrl &file_url);
    Q_INVOKABLE void submitOpenUrl(const QString &url_text);
    Q_INVOKABLE void submitNetworkSettings(const QString &user_agent, const QString &epg_url);
    Q_INVOKABLE void openLogsFolder();
    Q_INVOKABLE void copyDiagnosticsToClipboard();
    Q_INVOKABLE void dismissAlert();
    Q_INVOKABLE void openRecentAt(int index);
    Q_INVOKABLE void toggleSpeechSubtitleEnabled();
    Q_INVOKABLE void refreshSpeechModelStatus();
    Q_INVOKABLE void downloadSpeechModel();
    Q_INVOKABLE void cancelSpeechModelDownload();
    Q_INVOKABLE void installSpeechModelArchive(const QUrl &archive_url);
    Q_INVOKABLE void deleteSpeechModel();

   signals:
    void AvailableGroupsChanged();
    void CurrentGroupFilterChanged();
    void SearchTextChanged();
    void RecentItemsChanged();
    void StatusMessageChanged();
    void CurrentChannelNameChanged();
    void HasProgrammeInfoChanged();
    void CurrentProgrammeTitleChanged();
    void CurrentProgrammeTimeTextChanged();
    void CurrentProgrammeProgressChanged();
    void CurrentProgrammeProgressAvailableChanged();
    void NextProgrammeTitleChanged();
    void NextProgrammeTimeTextChanged();
    void PlaybackStateTextChanged();
    void PlaybackStateTokenChanged();
    void PlayingChanged();
    void MutedChanged();
    void VolumeChanged();
    void ConfiguredUserAgentChanged();
    void ConfiguredEpgUrlChanged();
    void LogFilePathChanged();
    void LogsDirectoryPathChanged();
    void AlertMessageChanged();
    void AlertVisibleChanged();
    void SpeechSubtitleTextChanged();
    void SpeechSubtitleEnabledChanged();
    void SpeechSubtitleAvailableChanged();
    void SpeechSubtitleUnavailableReasonChanged();
    void SpeechSubtitleActiveChanged();
    void SpeechSubtitleFinalChanged();
    void SpeechSubtitleLatencyMsChanged();
    void SpeechSubtitleStatusTextChanged();
    void SpeechModelStatusChanged();
    void SpeechModelBusyChanged();
    void SpeechModelOperationChanged();

    void ActivateChannelRequested(const QModelIndex &index);
    void PlayPauseRequested();
    void StopRequested();
    void MuteRequested(bool muted);
    void VolumeRequested(int volume);
    void OpenFileRequested(const QString &path);
    void OpenUrlRequested(const QString &url_text);
    void NetworkSettingsRequested(const QString &user_agent, const QString &epg_url);
    void OpenLogsFolderRequested();
    void CopyDiagnosticsRequested();
    void RecentOpenRequested(const QString &request_kind, const QString &target);
    void SpeechSubtitleEnabledRequested(bool enabled);
    void SpeechModelStatusRefreshRequested();
    void SpeechModelDownloadRequested();
    void SpeechModelDownloadCancelRequested();
    void SpeechModelArchiveInstallRequested(const QString &archive_path);
    void SpeechModelDeleteRequested();

   private:
    ui::models::ChannelFilterModel *channel_model_ = nullptr;
    QStringList available_groups_;
    QString current_group_filter_;
    QString search_text_;
    QVariantList recent_items_;
    QString status_message_;
    QString current_channel_name_;
    app::EpgProgrammePresentation programme_presentation_;
    QString playback_state_text_;
    QString playback_state_token_;
    bool playing_ = false;
    bool muted_ = false;
    int volume_ = 50;
    QString configured_user_agent_;
    QString configured_epg_url_;
    QString log_file_path_;
    QString logs_directory_path_;
    QString alert_message_;
    bool alert_visible_ = false;
    QString speech_subtitle_text_;
    bool speech_subtitle_enabled_ = false;
    bool speech_subtitle_available_ = false;
    QString speech_subtitle_unavailable_reason_;
    bool speech_subtitle_active_ = false;
    bool speech_subtitle_final_ = false;
    qint64 speech_subtitle_latency_ms_ = -1;
    QString speech_subtitle_status_text_;
    QString speech_model_status_token_;
    QString speech_model_status_text_;
    QString speech_model_status_detail_;
    QString speech_model_name_;
    QString speech_model_version_;
    QString speech_model_source_url_;
    QString speech_model_archive_size_text_;
    QString speech_model_installed_size_text_;
    QString speech_model_checksum_;
    QString speech_model_license_;
    QString speech_model_attribution_;
    QString speech_model_directory_;
    bool speech_model_installed_ = false;
    bool speech_model_developer_override_ = false;
    bool speech_model_runtime_available_ = false;
    bool speech_model_install_supported_ = false;
    bool speech_model_busy_ = false;
    bool speech_model_download_active_ = false;
    double speech_model_progress_ = -1.0;
    QString speech_model_operation_text_;
};

}  // namespace shatv::ui::shell
