#pragma once

#include <QLabel>
#include <QPushButton>
#include <QWidget>

#include "domain/player_snapshot.h"

namespace shatv::ui::panels {

class PlayerControlBar;

class PlaybackOsdOverlay final : public QWidget {
    Q_OBJECT

   public:
    explicit PlaybackOsdOverlay(QWidget *parent = nullptr);

    void ApplySnapshot(const domain::PlayerSnapshot &snapshot);

   signals:
    void PlayPauseRequested();
    void StopRequested();
    void MuteToggled(bool muted);
    void VolumeChanged(int volume);
    void ExitFullscreenRequested();

   private:
    QLabel *title_label_ = nullptr;
    QLabel *state_label_ = nullptr;
    QPushButton *exit_fullscreen_button_ = nullptr;
    PlayerControlBar *control_bar_ = nullptr;
};

}  // namespace shatv::ui::panels
