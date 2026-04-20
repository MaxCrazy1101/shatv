#include "ui/windows/main_window_bridge.h"

#include <algorithm>

#include <QVariantMap>

#include "domain/playback_state.h"
#include "ui/models/channel_filter_model.h"

namespace shatv::ui::windows {

MainWindowBridge::MainWindowBridge(ui::models::ChannelFilterModel *channel_model, QObject *parent)
    : QObject(parent), channel_model_(channel_model) {
    Q_ASSERT(channel_model_ != nullptr);
    setObjectName(QStringLiteral("mainWindowBridge"));
    current_group_filter_ = channel_model_->GroupFilter();
    search_text_ = channel_model_->SearchText();
}

QAbstractItemModel *MainWindowBridge::ChannelModel() const {
    return channel_model_;
}

QStringList MainWindowBridge::AvailableGroups() const {
    return available_groups_;
}
 
QString MainWindowBridge::CurrentGroupFilter() const {
    return current_group_filter_;
}

QString MainWindowBridge::SearchText() const {
    return search_text_;
}

QVariantList MainWindowBridge::RecentItems() const {
    return recent_items_;
}

bool MainWindowBridge::FullscreenActive() const {
    return fullscreen_active_;
}

QString MainWindowBridge::StatusMessage() const {
    return status_message_;
}

QString MainWindowBridge::CurrentChannelName() const {
    return current_channel_name_;
}

QString MainWindowBridge::PlaybackStateText() const {
    return playback_state_text_;
}

bool MainWindowBridge::Playing() const {
    return playing_;
}

bool MainWindowBridge::Muted() const {
    return muted_;
}

int MainWindowBridge::Volume() const {
    return volume_;
}

void MainWindowBridge::SetAvailableGroups(QStringList groups) {
    if (available_groups_ == groups) {
        return;
    }

    available_groups_ = std::move(groups);
    emit AvailableGroupsChanged();
}

void MainWindowBridge::SetCurrentGroupFilter(const QString &group) {
    if (current_group_filter_ == group) {
        return;
    }

    current_group_filter_ = group;
    emit CurrentGroupFilterChanged();
}

void MainWindowBridge::SetSearchTextValue(const QString &search_text) {
    if (search_text_ == search_text) {
        return;
    }

    search_text_ = search_text;
    emit SearchTextChanged();
}

void MainWindowBridge::SetRecentItems(const std::vector<app::RecentOpenItem> &items) {
    QVariantList next_items;
    next_items.reserve(static_cast<qsizetype>(items.size()));
    for (const auto &item : items) {
        QVariantMap map;
        map.insert(QStringLiteral("kind"), item.kind);
        map.insert(QStringLiteral("target"), item.target);
        map.insert(QStringLiteral("label"), item.label);
        next_items.push_back(map);
    }

    if (recent_items_ == next_items) {
        return;
    }

    recent_items_ = std::move(next_items);
    emit RecentItemsChanged();
}

void MainWindowBridge::SetFullscreenActive(bool active) {
    if (fullscreen_active_ == active) {
        return;
    }

    fullscreen_active_ = active;
    emit FullscreenActiveChanged();
}

void MainWindowBridge::SetStatusMessage(const QString &message) {
    if (status_message_ == message) {
        return;
    }

    status_message_ = message;
    emit StatusMessageChanged();
}

void MainWindowBridge::SetPlaybackSnapshot(const domain::PlayerSnapshot &snapshot) {
    const QString state_text = domain::PlaybackStateName(snapshot.state);
    const bool playing = snapshot.state == domain::PlaybackState::kPlaying;

    if (current_channel_name_ != snapshot.channel_name) {
        current_channel_name_ = snapshot.channel_name;
        emit CurrentChannelNameChanged();
    }
    if (playback_state_text_ != state_text) {
        playback_state_text_ = state_text;
        emit PlaybackStateTextChanged();
    }
    if (playing_ != playing) {
        playing_ = playing;
        emit PlayingChanged();
    }
    if (muted_ != snapshot.muted) {
        muted_ = snapshot.muted;
        emit MutedChanged();
    }
    if (volume_ != snapshot.volume) {
        volume_ = snapshot.volume;
        emit VolumeChanged();
    }
}

void MainWindowBridge::activateChannelRow(int row) {
    const QModelIndex model_index = channel_model_->index(row, 0);
    if (!model_index.isValid()) {
        return;
    }

    emit ActivateChannelRequested(model_index);
}

void MainWindowBridge::setSearchText(const QString &search_text) {
    if (search_text_ == search_text) {
        return;
    }

    SetSearchTextValue(search_text);
    channel_model_->SetSearchText(search_text);
}

void MainWindowBridge::setGroupFilter(const QString &group) {
    const QString normalized = group.trimmed();
    if (current_group_filter_ == normalized) {
        return;
    }

    SetCurrentGroupFilter(normalized);
    channel_model_->SetGroupFilter(normalized);
}

void MainWindowBridge::requestPlayPause() {
    emit PlayPauseRequested();
}

void MainWindowBridge::requestStop() {
    emit StopRequested();
}

void MainWindowBridge::toggleMute() {
    emit MuteRequested(!muted_);
}

void MainWindowBridge::setVolume(int volume) {
    const int clamped_volume = std::clamp(volume, 0, 100);
    if (volume_ == clamped_volume) {
        return;
    }

    emit VolumeRequested(clamped_volume);
}

void MainWindowBridge::requestOpenFile() {
    emit OpenFileRequested();
}

void MainWindowBridge::requestOpenUrl() {
    emit OpenUrlRequested();
}

void MainWindowBridge::requestNetworkSettings() {
    emit NetworkSettingsRequested();
}

void MainWindowBridge::requestAbout() {
    emit AboutRequested();
}

void MainWindowBridge::openRecentAt(int index) {
    if (index < 0 || index >= recent_items_.size()) {
        return;
    }

    const QVariantMap item = recent_items_.at(index).toMap();
    emit RecentOpenRequested(item.value(QStringLiteral("kind")).toString(),
                             item.value(QStringLiteral("target")).toString());
}

void MainWindowBridge::toggleFullscreen() {
    emit ToggleFullscreenRequested();
}

void MainWindowBridge::exitFullscreen() {
    emit ExitFullscreenRequested();
}

}  // namespace shatv::ui::windows
