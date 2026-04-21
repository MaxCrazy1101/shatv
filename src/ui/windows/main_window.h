#pragma once

#include <vector>

#include <QKeyEvent>
#include <QMainWindow>
#include <QPointer>
#include <QString>
#include <QTimer>

#include "app/app_settings.h"
#include "domain/channel.h"
#include "domain/player_snapshot.h"

class QQuickWidget;
class QWidget;

namespace shatv::application {
class PlayerController;
}

namespace shatv::player {
class MpvPlayerBackend;
}

namespace shatv::ui::models {
class ChannelFilterModel;
class ChannelListModel;
}

namespace shatv::ui::qml_spike {
class MpvVideoItem;
}

namespace shatv::ui::windows {

class MainWindowBridge;

class MainWindow final : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(application::PlayerController *controller, ui::models::ChannelListModel *channel_model,
                        QWidget *parent = nullptr);
    ~MainWindow() override;

    void SetChannels(std::vector<domain::Channel> channels);
    void StartInitialPlayback();
    void StartSmokeScenario();
    void AttachMpvBackend(player::MpvPlayerBackend *backend);
    void SetConfiguredUserAgent(const QString &user_agent);
    void SetOsdAutoHideSeconds(int seconds);
    void SetRecentItems(std::vector<app::RecentOpenItem> items);
    void ShowStatusMessage(const QString &message, int timeout_ms = 3000);

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
    void ToggleFullscreen();
    void ExitFullscreen();
    void OnAboutRequested();

  private:
    void keyPressEvent(QKeyEvent *event) override;
    void BuildUi();
    void ApplyFullscreenUiState(bool active);
    void RebuildGroupFilter();

    application::PlayerController *controller_ = nullptr;
    ui::models::ChannelListModel *channel_model_ = nullptr;
    ui::models::ChannelFilterModel *channel_filter_model_ = nullptr;
    QQuickWidget *qml_view_ = nullptr;
    QPointer<QObject> qml_root_object_;
    QPointer<ui::qml_spike::MpvVideoItem> video_item_;
    MainWindowBridge *bridge_ = nullptr;
    std::vector<app::RecentOpenItem> recent_items_;
    domain::PlayerSnapshot last_snapshot_;
    QString configured_user_agent_;
    QString status_message_;
    QTimer status_message_timer_;
    bool fullscreen_active_ = false;
    bool was_maximized_before_fullscreen_ = false;
};

}  // namespace shatv::ui::windows
