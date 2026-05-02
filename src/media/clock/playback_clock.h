#pragma once

#include <atomic>

#include <QtGlobal>

namespace shatv::media::clock {

// 音频主时钟只暴露线程安全的微秒位置，避免解码线程直接访问 QAudioSink。
class PlaybackClock final {
   public:
    PlaybackClock() = default;
    ~PlaybackClock() = default;

    PlaybackClock(const PlaybackClock &) = delete;
    PlaybackClock &operator=(const PlaybackClock &) = delete;

    void Reset() {
        position_usecs_.store(0, std::memory_order_relaxed);
    }

    void SetPositionUsecs(qint64 position_usecs) {
        position_usecs_.store(position_usecs, std::memory_order_relaxed);
    }

    qint64 PositionUsecs() const {
        return position_usecs_.load(std::memory_order_relaxed);
    }

   private:
    std::atomic<qint64> position_usecs_ = 0;
};

}  // namespace shatv::media::clock
