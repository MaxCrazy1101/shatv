#pragma once

#include <vector>

#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListView>
#include <QMainWindow>
#include <QMenu>
#include <QString>

#include "app/app_settings.h"
#include "domain/channel.h"
#include "domain/player_snapshot.h"

class QAction;

namespace shatv::application {
class PlayerController;
}

namespace shatv::player {
class MpvRenderWidget;
}

namespace shatv::ui::models {
class ChannelFilterModel;
class ChannelListModel;
}

namespace shatv::ui::panels {
class PlayerControlBar;
class PlaybackStatusPanel;
}

namespace shatv::ui::widgets {
class PlaybackViewport;
}

namespace shatv::ui::windows {

class MainWindow final : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(application::PlayerController *controller, ui::models::ChannelListModel *channel_model,
                        QWidget *parent = nullptr);

    void SetChannels(std::vector<domain::Channel> channels);
    void StartInitialPlayback();
    void StartSmokeScenario();
    player::MpvRenderWidget *RenderWidget() const;
    void SetConfiguredUserAgent(const QString &user_agent);
    void SetOsdAutoHideSeconds(int seconds);
    void SetRecentItems(std::vector<app::RecentOpenItem> items);

    int ChannelCount() const;
    bool IsFullscreenModeActive() const;
    QString CurrentChannelIdForSmoke() const;
    domain::PlayerSnapshot LastAppliedSnapshot() const;

   signals:
    void OpenFileSelected(const QString &path);
    void OpenUrlSelected(const QString &url_text);
    void RecentOpenSelected(const QString &kind, const QString &target);
    void UserAgentChanged(const QString &user_agent);
    void UiSnapshotApplied(const shatv::domain::PlayerSnapshot &snapshot);

   private slots:
    void OnChannelActivated(const QModelIndex &index);
    void OnPlaybackSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot);
    void OnPlayPauseRequested();
    void OnMuteToggled(bool muted);
    void OnOpenFileRequested();
    void OnOpenUrlRequested();
    void OnNetworkSettingsRequested();
    void OnGroupFilterChanged(int index);
    void ToggleFullscreen();
    void ExitFullscreen();
    void OnAboutRequested();

  private:
    void keyPressEvent(QKeyEvent *event) override;
    void BuildUi();
    void ApplyFullscreenUiState(bool active);
    void RebuildGroupFilter();
    void RebuildRecentMenu();

    application::PlayerController *controller_ = nullptr;
    ui::models::ChannelListModel *channel_model_ = nullptr;
    ui::models::ChannelFilterModel *channel_filter_model_ = nullptr;
    QWidget *left_panel_ = nullptr;
    QListView *channel_list_view_ = nullptr;
    QLineEdit *search_input_ = nullptr;
    QComboBox *group_filter_ = nullptr;
    ui::widgets::PlaybackViewport *playback_viewport_ = nullptr;
    panels::PlayerControlBar *control_bar_ = nullptr;
    panels::PlaybackStatusPanel *status_panel_ = nullptr;
    QMenu *recent_menu_ = nullptr;
    QAction *toggle_fullscreen_action_ = nullptr;
    std::vector<app::RecentOpenItem> recent_items_;
    domain::PlayerSnapshot last_snapshot_;
    QString configured_user_agent_;
    bool fullscreen_active_ = false;
    bool was_maximized_before_fullscreen_ = false;
};

}  // namespace shatv::ui::windows
