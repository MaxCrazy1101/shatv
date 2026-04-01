#pragma once

#include <vector>

#include <QComboBox>
#include <QLineEdit>
#include <QListView>
#include <QMainWindow>

#include "domain/channel.h"
#include "domain/player_snapshot.h"

namespace shatv::application {
class PlayerController;
}

namespace shatv::player {
class MpvRenderWidget;
}

namespace shatv::ui::models {
class ChannelListModel;
}

namespace shatv::ui::panels {
class PlayerControlBar;
class PlaybackStatusPanel;
}

namespace shatv::ui::windows {

class MainWindow final : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(application::PlayerController *controller, ui::models::ChannelListModel *channel_model,
                        QWidget *parent = nullptr);

    void SetChannels(std::vector<domain::Channel> channels);
    void StartSmokeScenario();

    int ChannelCount() const;
    QString CurrentChannelIdForSmoke() const;
    domain::PlayerSnapshot LastAppliedSnapshot() const;

   signals:
    void UiSnapshotApplied(const shatv::domain::PlayerSnapshot &snapshot);

   private slots:
    void OnChannelActivated(const QModelIndex &index);
    void OnPlaybackSnapshotChanged(const shatv::domain::PlayerSnapshot &snapshot);
    void OnPlayPauseRequested();
    void OnMuteToggled(bool muted);

   private:
    void BuildUi();

    application::PlayerController *controller_ = nullptr;
    ui::models::ChannelListModel *channel_model_ = nullptr;
    QListView *channel_list_view_ = nullptr;
    QLineEdit *search_input_ = nullptr;
    QComboBox *group_filter_ = nullptr;
    player::MpvRenderWidget *render_widget_ = nullptr;
    panels::PlayerControlBar *control_bar_ = nullptr;
    panels::PlaybackStatusPanel *status_panel_ = nullptr;
    domain::PlayerSnapshot last_snapshot_;
};

}  // namespace shatv::ui::windows
