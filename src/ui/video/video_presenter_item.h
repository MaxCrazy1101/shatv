#pragma once

#include <QQuickRhiItem>

#include "media/video/video_frame.h"
#include "player/video_frame_sink.h"

namespace shatv::player {
class FfmpegPlayerBackend;
}

namespace shatv::ui::video {

class VideoPresenterItem : public QQuickRhiItem, public shatv::player::VideoFrameSink {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

   public:
    explicit VideoPresenterItem(QQuickItem *parent = nullptr);
    ~VideoPresenterItem() override;

    bool ready() const;
    void SetBackend(shatv::player::FfmpegPlayerBackend *backend);
    shatv::player::FfmpegPlayerBackend *Backend() const;
    QQuickRhiItemRenderer *createRenderer() override;

    void PresentVideoFrame(const media::video::VideoFrame &frame) override;
    bool TakePendingFrame(media::video::VideoFrame *frame);

   signals:
    void readyChanged();

   private:
    void ApplyReady(bool ready);

    shatv::player::FfmpegPlayerBackend *backend_ = nullptr;
    media::video::VideoFrame pending_frame_;
    bool has_pending_frame_ = false;
    bool ready_ = false;
};

void RegisterQmlVideoTypes();

}  // namespace shatv::ui::video
