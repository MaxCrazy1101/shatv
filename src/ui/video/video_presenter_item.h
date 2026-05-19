#pragma once

#include <QObject>
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
    Q_PROPERTY(VideoAspectRatioMode aspectRatioMode READ aspectRatioMode WRITE setAspectRatioMode NOTIFY
                   aspectRatioModeChanged)

   public:
    enum VideoAspectRatioMode {
        PreserveAspectRatio,  // Fit inside viewport, preserving aspect ratio.
        StretchToFill,        // Stretch to fill viewport without preserving aspect ratio.
        CropToFill,           // Fill viewport while preserving aspect ratio and cropping overflow.
        NativeSize,           // Present without aspect-ratio correction.
    };
    Q_ENUM(VideoAspectRatioMode)

    explicit VideoPresenterItem(QQuickItem *parent = nullptr);
    ~VideoPresenterItem() override;

    bool ready() const;
    VideoAspectRatioMode aspectRatioMode() const;
    void setAspectRatioMode(VideoAspectRatioMode mode);

    void SetBackend(shatv::player::FfmpegPlayerBackend *backend);
    shatv::player::FfmpegPlayerBackend *Backend() const;
    QQuickRhiItemRenderer *createRenderer() override;

    void PresentVideoFrame(const media::video::VideoFrame &frame) override;
    bool TakePendingFrame(media::video::VideoFrame *frame);

   signals:
    void readyChanged();
    void aspectRatioModeChanged();

   private:
    void ApplyReady(bool ready);

    shatv::player::FfmpegPlayerBackend *backend_ = nullptr;
    media::video::VideoFrame pending_frame_;
    bool has_pending_frame_ = false;
    bool ready_ = false;
    VideoAspectRatioMode aspect_ratio_mode_ = PreserveAspectRatio;
};

void RegisterQmlVideoTypes();

}  // namespace shatv::ui::video
