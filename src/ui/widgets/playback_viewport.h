#pragma once

#include <QTimer>
#include <QWidget>

#include "domain/player_snapshot.h"

namespace shatv::player {
class MpvRenderWidget;
}

namespace shatv::ui::panels {
class PlaybackOsdOverlay;
}

namespace shatv::ui::widgets {

class PlaybackViewport final : public QWidget {
    Q_OBJECT

   public:
    explicit PlaybackViewport(QWidget *parent = nullptr);

    void ApplySnapshot(const domain::PlayerSnapshot &snapshot);
    void SetFullscreenActive(bool active);
    void SetOsdAutoHideSeconds(int seconds);
    void RevealOsd();
    bool IsOsdVisible() const;
    player::MpvRenderWidget *RenderWidget() const;
    panels::PlaybackOsdOverlay *OsdOverlay() const;

   signals:
    void PlayPauseRequested();
    void StopRequested();
    void MuteToggled(bool muted);
    void VolumeChanged(int volume);
    void ExitFullscreenRequested();

   protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

   private:
    void RegisterActivitySource(QWidget *widget);
    void RestartHideTimer();
    void HideOsd();

    player::MpvRenderWidget *render_widget_ = nullptr;
    panels::PlaybackOsdOverlay *osd_overlay_ = nullptr;
    QTimer *osd_hide_timer_ = nullptr;
    bool fullscreen_active_ = false;
    int osd_auto_hide_seconds_ = 3;
};

}  // namespace shatv::ui::widgets
