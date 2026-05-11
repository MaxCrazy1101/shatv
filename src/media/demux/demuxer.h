#pragma once

#include <atomic>

#include <QtGlobal>
#include <QString>

#include "domain/media_source.h"

struct AVCodecParameters;
struct AVFormatContext;
struct AVPacket;

namespace shatv::media::demux {

enum class ReadPacketResult {
    kPacket,
    kEndOfFile,
    kError,
};

struct DemuxerOpenOptions {
    const std::atomic_bool *abort_requested = nullptr;
};

// Opens media with libavformat and yields packets for selected streams.
class Demuxer final {
   public:
    Demuxer() = default;
    ~Demuxer();

    Demuxer(const Demuxer &) = delete;
    Demuxer &operator=(const Demuxer &) = delete;

    bool Open(const domain::MediaSourceDescriptor &source, QString *error_message);
    bool Open(const domain::MediaSourceDescriptor &source,
              DemuxerOpenOptions options,
              QString *error_message);
    void Close();

    ReadPacketResult ReadNextAudioPacket(AVPacket *packet, QString *error_message);
    ReadPacketResult ReadNextVideoPacket(AVPacket *packet, QString *error_message);
    ReadPacketResult ReadNextMediaPacket(AVPacket *packet, QString *error_message);
    const AVCodecParameters *AudioCodecParameters() const;
    const AVCodecParameters *VideoCodecParameters() const;
    double AudioTimeBaseSeconds() const;
    double VideoTimeBaseSeconds() const;
    int AudioStreamIndex() const;
    int VideoStreamIndex() const;
    bool HasAudioStream() const;
    bool HasVideoStream() const;
    bool IsAudioPacket(const AVPacket &packet) const;
    bool IsVideoPacket(const AVPacket &packet) const;

   private:
    enum class InterruptOperation {
        kNone,
        kOpen,
        kRead,
    };

    enum class InterruptReason {
        kNone,
        kAbortRequested,
        kOpenTimeout,
        kReadTimeout,
    };

    struct RemoteTimeoutConfig {
        bool enabled = false;
        int open_timeout_ms = 0;
        int read_timeout_ms = 0;
    };

    static int InterruptCallback(void *opaque);

    bool ConfigureRemoteTimeouts(const domain::MediaSourceDescriptor &source,
                                 RemoteTimeoutConfig *config,
                                 QString *error_message) const;
    void BeginInterruptibleOperation(InterruptOperation operation, int timeout_ms);
    void EndInterruptibleOperation();
    bool ShouldInterrupt();
    QString InterruptErrorMessage() const;
    int ReadFrame(AVPacket *packet);
    ReadPacketResult ReadNextPacketForStream(int stream_index, AVPacket *packet, QString *error_message);

    AVFormatContext *format_context_ = nullptr;
    int audio_stream_index_ = -1;
    int video_stream_index_ = -1;
    const std::atomic_bool *abort_requested_ = nullptr;
    RemoteTimeoutConfig remote_timeout_config_;
    std::atomic_int interrupt_operation_ = static_cast<int>(InterruptOperation::kNone);
    std::atomic_int interrupt_reason_ = static_cast<int>(InterruptReason::kNone);
    std::atomic<qint64> interrupt_deadline_ms_ = 0;
};

}  // namespace shatv::media::demux
