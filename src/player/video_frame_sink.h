#pragma once

#include "media/video/video_frame.h"

namespace shatv::player {

class VideoFrameSink {
   public:
    virtual ~VideoFrameSink() = default;
    virtual void PresentVideoFrame(const media::video::VideoFrame &frame) = 0;
};

}  // namespace shatv::player
