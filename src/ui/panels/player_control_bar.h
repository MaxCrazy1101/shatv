#pragma once

#include <QPushButton>
#include <QSlider>
#include <QWidget>

#include "domain/player_snapshot.h"

namespace shatv::ui::panels {

class PlayerControlBar final : public QWidget {
    Q_OBJECT

   public:
    explicit PlayerControlBar(QWidget *parent = nullptr);

    void ApplySnapshot(const domain::PlayerSnapshot &snapshot);

   signals:
    void PlayPauseRequested();
    void StopRequested();
    void MuteToggled(bool muted);
    void VolumeChanged(int volume);

   private:
    QPushButton *play_pause_button_ = nullptr;
    QPushButton *stop_button_ = nullptr;
    QPushButton *mute_button_ = nullptr;
    QSlider *volume_slider_ = nullptr;
    bool muted_ = false;
};

}  // namespace shatv::ui::panels
