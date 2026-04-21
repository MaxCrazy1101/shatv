#pragma once

#include <vector>

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QObject>
#include <QStringList>
#include <QVariantList>

#include "app/app_settings.h"
#include "domain/player_snapshot.h"

namespace shatv::ui::models {
class ChannelFilterModel;
}

namespace shatv::ui::windows {

class MainWindowBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *channelModel READ ChannelModel CONSTANT)
    Q_PROPERTY(QStringList availableGroups READ AvailableGroups NOTIFY AvailableGroupsChanged)
    Q_PROPERTY(QString currentGroupFilter READ CurrentGroupFilter NOTIFY CurrentGroupFilterChanged)
    Q_PROPERTY(QString searchText READ SearchText NOTIFY SearchTextChanged)
    Q_PROPERTY(QVariantList recentItems READ RecentItems NOTIFY RecentItemsChanged)
    Q_PROPERTY(bool fullscreenActive READ FullscreenActive NOTIFY FullscreenActiveChanged)
    Q_PROPERTY(QString statusMessage READ StatusMessage NOTIFY StatusMessageChanged)
    Q_PROPERTY(QString currentChannelName READ CurrentChannelName NOTIFY CurrentChannelNameChanged)
    Q_PROPERTY(QString playbackStateText READ PlaybackStateText NOTIFY PlaybackStateTextChanged)
    Q_PROPERTY(QString playbackStateToken READ PlaybackStateToken NOTIFY PlaybackStateTokenChanged)
    Q_PROPERTY(bool playing READ Playing NOTIFY PlayingChanged)
    Q_PROPERTY(bool muted READ Muted NOTIFY MutedChanged)
    Q_PROPERTY(int volume READ Volume NOTIFY VolumeChanged)

   public:
    explicit MainWindowBridge(ui::models::ChannelFilterModel *channel_model, QObject *parent = nullptr);

    QAbstractItemModel *ChannelModel() const;
    QStringList AvailableGroups() const;
    QString CurrentGroupFilter() const;
    QString SearchText() const;
    QVariantList RecentItems() const;
    bool FullscreenActive() const;
    QString StatusMessage() const;
    QString CurrentChannelName() const;
    QString PlaybackStateText() const;
    QString PlaybackStateToken() const;
    bool Playing() const;
    bool Muted() const;
    int Volume() const;

    void SetAvailableGroups(QStringList groups);
    void SetCurrentGroupFilter(const QString &group);
    void SetSearchTextValue(const QString &search_text);
    void SetRecentItems(const std::vector<app::RecentOpenItem> &items);
    void SetFullscreenActive(bool active);
    void SetStatusMessage(const QString &message);
    void SetPlaybackSnapshot(const domain::PlayerSnapshot &snapshot);

    Q_INVOKABLE void activateChannelRow(int row);
    Q_INVOKABLE void setSearchText(const QString &search_text);
    Q_INVOKABLE void setGroupFilter(const QString &group);
    Q_INVOKABLE void requestPlayPause();
    Q_INVOKABLE void requestStop();
    Q_INVOKABLE void toggleMute();
    Q_INVOKABLE void setVolume(int volume);
    Q_INVOKABLE void requestOpenFile();
    Q_INVOKABLE void requestOpenUrl();
    Q_INVOKABLE void requestNetworkSettings();
    Q_INVOKABLE void requestAbout();
    Q_INVOKABLE void openRecentAt(int index);
    Q_INVOKABLE void toggleFullscreen();
    Q_INVOKABLE void exitFullscreen();

   signals:
    void AvailableGroupsChanged();
    void CurrentGroupFilterChanged();
    void SearchTextChanged();
    void RecentItemsChanged();
    void FullscreenActiveChanged();
    void StatusMessageChanged();
    void CurrentChannelNameChanged();
    void PlaybackStateTextChanged();
    void PlaybackStateTokenChanged();
    void PlayingChanged();
    void MutedChanged();
    void VolumeChanged();

    void ActivateChannelRequested(const QModelIndex &index);
    void PlayPauseRequested();
    void StopRequested();
    void MuteRequested(bool muted);
    void VolumeRequested(int volume);
    void OpenFileRequested();
    void OpenUrlRequested();
    void NetworkSettingsRequested();
    void AboutRequested();
    void RecentOpenRequested(const QString &kind, const QString &target);
    void ToggleFullscreenRequested();
    void ExitFullscreenRequested();

   private:
    ui::models::ChannelFilterModel *channel_model_ = nullptr;
    QStringList available_groups_;
    QString current_group_filter_;
    QString search_text_;
    QVariantList recent_items_;
    bool fullscreen_active_ = false;
    QString status_message_;
    QString current_channel_name_;
    QString playback_state_text_;
    QString playback_state_token_;
    bool playing_ = false;
    bool muted_ = false;
    int volume_ = 50;
};

}  // namespace shatv::ui::windows
