#include "ui/shell/app_shell_bridge.h"

#include <algorithm>

#include <QVariantMap>

#include "app/build_info.h"
#include "domain/playback_state.h"
#include "ui/models/channel_filter_model.h"

namespace shatv::ui::shell {

AppShellBridge::AppShellBridge(ui::models::ChannelFilterModel *channel_model, QObject *parent)
    : QObject(parent), channel_model_(channel_model) {
    Q_ASSERT(channel_model_ != nullptr);
    setObjectName(QStringLiteral("appShellBridge"));
    current_group_filter_ = channel_model_->GroupFilter();
    search_text_ = channel_model_->SearchText();
}

QAbstractItemModel *AppShellBridge::ChannelModel() const {
    return channel_model_;
}

QStringList AppShellBridge::AvailableGroups() const {
    return available_groups_;
}

QString AppShellBridge::CurrentGroupFilter() const {
    return current_group_filter_;
}

QString AppShellBridge::SearchText() const {
    return search_text_;
}

QVariantList AppShellBridge::RecentItems() const {
    return recent_items_;
}

QString AppShellBridge::StatusMessage() const {
    return status_message_;
}

QString AppShellBridge::CurrentChannelName() const {
    return current_channel_name_;
}

QString AppShellBridge::CurrentProgrammeText() const {
    return current_programme_text_;
}

QString AppShellBridge::NextProgrammeText() const {
    return next_programme_text_;
}

QString AppShellBridge::PlaybackStateText() const {
    return playback_state_text_;
}

QString AppShellBridge::PlaybackStateToken() const {
    return playback_state_token_;
}

bool AppShellBridge::Playing() const {
    return playing_;
}

bool AppShellBridge::Muted() const {
    return muted_;
}

int AppShellBridge::Volume() const {
    return volume_;
}

QString AppShellBridge::ConfiguredUserAgent() const {
    return configured_user_agent_;
}

QString AppShellBridge::ConfiguredEpgUrl() const {
    return configured_epg_url_;
}

QString AppShellBridge::AppVersion() const {
    return QString::fromUtf8(app::kProjectVersion);
}

QString AppShellBridge::BuildId() const {
    return QString::fromUtf8(app::kBuildId);
}

QString AppShellBridge::LogFilePath() const {
    return log_file_path_;
}

QString AppShellBridge::LogsDirectoryPath() const {
    return logs_directory_path_;
}

QString AppShellBridge::AlertMessage() const {
    return alert_message_;
}

bool AppShellBridge::AlertVisible() const {
    return alert_visible_;
}

void AppShellBridge::SetAvailableGroups(QStringList groups) {
    if (available_groups_ == groups) {
        return;
    }

    available_groups_ = std::move(groups);
    emit AvailableGroupsChanged();
}

void AppShellBridge::SetCurrentGroupFilter(const QString &group) {
    if (current_group_filter_ == group) {
        return;
    }

    current_group_filter_ = group;
    emit CurrentGroupFilterChanged();
}

void AppShellBridge::SetSearchTextValue(const QString &search_text) {
    if (search_text_ == search_text) {
        return;
    }

    search_text_ = search_text;
    emit SearchTextChanged();
}

void AppShellBridge::SetRecentItems(const std::vector<app::RecentOpenItem> &items) {
    QVariantList next_items;
    next_items.reserve(static_cast<qsizetype>(items.size()));
    for (const auto &item : items) {
        QVariantMap map;
        map.insert(QStringLiteral("requestKind"), app::OpenRequestKindToken(item.request_kind));
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

void AppShellBridge::SetStatusMessage(const QString &message) {
    if (status_message_ == message) {
        return;
    }

    status_message_ = message;
    emit StatusMessageChanged();
}

void AppShellBridge::SetProgrammeTexts(const QString &current_programme_text, const QString &next_programme_text) {
    if (current_programme_text_ != current_programme_text) {
        current_programme_text_ = current_programme_text;
        emit CurrentProgrammeTextChanged();
    }
    if (next_programme_text_ != next_programme_text) {
        next_programme_text_ = next_programme_text;
        emit NextProgrammeTextChanged();
    }
}

void AppShellBridge::SetPlaybackSnapshot(const domain::PlayerSnapshot &snapshot) {
    const QString state_text = domain::PlaybackStateName(snapshot.state);
    const QString state_token = domain::PlaybackStateToken(snapshot.state);
    const bool playing = snapshot.state == domain::PlaybackState::kPlaying ||
                         snapshot.state == domain::PlaybackState::kLoading ||
                         snapshot.state == domain::PlaybackState::kBuffering ||
                         snapshot.state == domain::PlaybackState::kRetrying;

    if (current_channel_name_ != snapshot.channel_name) {
        current_channel_name_ = snapshot.channel_name;
        emit CurrentChannelNameChanged();
    }
    if (playback_state_text_ != state_text) {
        playback_state_text_ = state_text;
        emit PlaybackStateTextChanged();
    }
    if (playback_state_token_ != state_token) {
        playback_state_token_ = state_token;
        emit PlaybackStateTokenChanged();
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

void AppShellBridge::SetConfiguredUserAgent(const QString &user_agent) {
    if (configured_user_agent_ == user_agent) {
        return;
    }

    configured_user_agent_ = user_agent;
    emit ConfiguredUserAgentChanged();
}

void AppShellBridge::SetConfiguredEpgUrl(const QString &epg_url) {
    if (configured_epg_url_ == epg_url) {
        return;
    }

    configured_epg_url_ = epg_url;
    emit ConfiguredEpgUrlChanged();
}

void AppShellBridge::SetLogPaths(const QString &log_file_path, const QString &logs_directory_path) {
    if (log_file_path_ != log_file_path) {
        log_file_path_ = log_file_path;
        emit LogFilePathChanged();
    }
    if (logs_directory_path_ != logs_directory_path) {
        logs_directory_path_ = logs_directory_path;
        emit LogsDirectoryPathChanged();
    }
}

void AppShellBridge::SetAlertMessage(const QString &message) {
    const bool visible = !message.isEmpty();

    if (alert_message_ != message) {
        alert_message_ = message;
        emit AlertMessageChanged();
    }

    if (alert_visible_ != visible) {
        alert_visible_ = visible;
        emit AlertVisibleChanged();
    }
}

void AppShellBridge::activateChannelRow(int row) {
    const QModelIndex model_index = channel_model_->index(row, 0);
    if (!model_index.isValid()) {
        return;
    }

    emit ActivateChannelRequested(model_index);
}

void AppShellBridge::setSearchText(const QString &search_text) {
    if (search_text_ == search_text) {
        return;
    }

    SetSearchTextValue(search_text);
    channel_model_->SetSearchText(search_text);
}

void AppShellBridge::setGroupFilter(const QString &group) {
    const QString normalized = group.trimmed();
    if (current_group_filter_ == normalized) {
        return;
    }

    SetCurrentGroupFilter(normalized);
    channel_model_->SetGroupFilter(normalized);
}

void AppShellBridge::requestPlayPause() {
    emit PlayPauseRequested();
}

void AppShellBridge::requestStop() {
    emit StopRequested();
}

void AppShellBridge::toggleMute() {
    emit MuteRequested(!muted_);
}

void AppShellBridge::setVolume(int volume) {
    const int clamped_volume = std::clamp(volume, 0, 100);
    if (volume_ == clamped_volume) {
        return;
    }

    emit VolumeRequested(clamped_volume);
}

void AppShellBridge::submitOpenFile(const QUrl &file_url) {
    if (!file_url.isValid()) {
        return;
    }

    if (file_url.isLocalFile()) {
        const QString local_path = file_url.toLocalFile().trimmed();
        if (local_path.isEmpty()) {
            return;
        }

        emit OpenFileRequested(local_path);
        return;
    }

    const QString normalized_target = file_url.toString().trimmed();
    if (normalized_target.isEmpty()) {
        return;
    }

    emit OpenFileRequested(normalized_target);
}

void AppShellBridge::submitOpenUrl(const QString &url_text) {
    const QString normalized_url = url_text.trimmed();
    if (normalized_url.isEmpty()) {
        return;
    }

    emit OpenUrlRequested(normalized_url);
}

void AppShellBridge::submitNetworkSettings(const QString &user_agent, const QString &epg_url) {
    emit NetworkSettingsRequested(user_agent.trimmed(), epg_url.trimmed());
}

void AppShellBridge::openLogsFolder() {
    emit OpenLogsFolderRequested();
}

void AppShellBridge::copyDiagnosticsToClipboard() {
    emit CopyDiagnosticsRequested();
}

void AppShellBridge::dismissAlert() {
    SetAlertMessage(QString());
}

void AppShellBridge::openRecentAt(int index) {
    if (index < 0 || index >= recent_items_.size()) {
        return;
    }

    const QVariantMap item = recent_items_.at(index).toMap();
    emit RecentOpenRequested(item.value(QStringLiteral("requestKind")).toString(),
                             item.value(QStringLiteral("target")).toString());
}

}  // namespace shatv::ui::shell
