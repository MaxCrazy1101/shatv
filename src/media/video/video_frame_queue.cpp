#include "media/video/video_frame_queue.h"

#include <algorithm>
#include <utility>

#include <QMutexLocker>

namespace shatv::media::video {

VideoFrameQueue::VideoFrameQueue(std::size_t capacity) : capacity_(std::max<std::size_t>(1, capacity)) {}

void VideoFrameQueue::Push(VideoFrame frame) {
    QMutexLocker locker(&mutex_);
    while (frames_.size() >= capacity_) {
        frames_.pop_front();
    }
    frames_.push_back(std::move(frame));
}

bool VideoFrameQueue::TryPop(VideoFrame *frame) {
    if (frame == nullptr) {
        return false;
    }

    QMutexLocker locker(&mutex_);
    if (frames_.empty()) {
        return false;
    }

    *frame = std::move(frames_.front());
    frames_.pop_front();
    return true;
}

bool VideoFrameQueue::TryPeek(VideoFrame *frame) {
    if (frame == nullptr) {
        return false;
    }

    QMutexLocker locker(&mutex_);
    if (frames_.empty()) {
        return false;
    }

    *frame = frames_.front();
    return true;
}

bool VideoFrameQueue::DropOldest() {
    QMutexLocker locker(&mutex_);
    if (frames_.empty()) {
        return false;
    }

    frames_.pop_front();
    return true;
}

bool VideoFrameQueue::IsFull() const {
    QMutexLocker locker(&mutex_);
    return frames_.size() >= capacity_;
}

void VideoFrameQueue::Clear() {
    QMutexLocker locker(&mutex_);
    frames_.clear();
}

}  // namespace shatv::media::video
