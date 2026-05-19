#pragma once

#include <QMutex>
#include <cstddef>
#include <deque>

#include "media/video/video_frame.h"

namespace shatv::media::video {

// Bounded FIFO for decoded frames. When the producer outruns presentation, the
// oldest frame is dropped so memory use stays bounded.
class VideoFrameQueue final {
   public:
    explicit VideoFrameQueue(std::size_t capacity = 3);
    ~VideoFrameQueue() = default;

    VideoFrameQueue(const VideoFrameQueue &) = delete;
    VideoFrameQueue &operator=(const VideoFrameQueue &) = delete;

    void Push(VideoFrame frame);
    bool TryPeek(VideoFrame *frame);
    bool TryPop(VideoFrame *frame);
    bool DropOldest();
    bool IsFull() const;
    void Clear();

   private:
    std::size_t capacity_ = 3;
    mutable QMutex mutex_;
    std::deque<VideoFrame> frames_;
};

}  // namespace shatv::media::video
