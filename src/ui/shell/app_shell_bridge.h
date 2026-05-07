#pragma once

#include <vector>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

#include "app/app_settings.h"
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
    Q_PROPERTY(QString currentProgrammeText READ CurrentProgrammeText NOTIFY CurrentProgrammeTextChanged)
    Q_PROPERTY(QString nextProgrammeText READ NextProgrammeText NOTIFY NextProgrammeTextChanged)
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

   public:
    explicit AppShellBridge(ui::models::ChannelFilterModel *channel_model, QObject *parent = nullptr);

    QAbstractItemModel *ChannelModel() const;
    QStringList AvailableGroups() const;
    QString CurrentGroupFilter() const;
    QString SearchText() const;
    QVariantList RecentItems() const;
    QString StatusMessage() const;
    QString CurrentChannelName() const;
    QString CurrentProgrammeText() const;
    QString NextProgrammeText() const;
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

    void SetAvailableGroups(QStringList groups);
    void SetCurrentGroupFilter(const QString &group);
    void SetSearchTextValue(const QString &search_text);
    void SetRecentItems(const std::vector<app::RecentOpenItem> &items);
    void SetStatusMessage(const QString &message);
    void SetProgrammeTexts(const QString &current_programme_text, const QString &next_programme_text);
    void SetPlaybackSnapshot(const domain::PlayerSnapshot &snapshot);
    void SetConfiguredUserAgent(const QString &user_agent);
    void SetConfiguredEpgUrl(const QString &epg_url);
    void SetLogPaths(const QString &log_file_path, const QString &logs_directory_path);
    void SetAlertMessage(const QString &message);

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

   signals:
    void AvailableGroupsChanged();
    void CurrentGroupFilterChanged();
    void SearchTextChanged();
    void RecentItemsChanged();
    void StatusMessageChanged();
    void CurrentChannelNameChanged();
    void CurrentProgrammeTextChanged();
    void NextProgrammeTextChanged();
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

   private:
    ui::models::ChannelFilterModel *channel_model_ = nullptr;
    QStringList available_groups_;
    QString current_group_filter_;
    QString search_text_;
    QVariantList recent_items_;
    QString status_message_;
    QString current_channel_name_;
    QString current_programme_text_;
    QString next_programme_text_;
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
};

}  // namespace shatv::ui::shell
