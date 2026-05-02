#pragma once

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

// Opens media with libavformat and yields packets for selected streams.
class Demuxer final {
   public:
    Demuxer() = default;
    ~Demuxer();

    Demuxer(const Demuxer &) = delete;
    Demuxer &operator=(const Demuxer &) = delete;

    bool Open(const domain::MediaSourceDescriptor &source, QString *error_message);
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
    ReadPacketResult ReadNextPacketForStream(int stream_index, AVPacket *packet, QString *error_message);

    AVFormatContext *format_context_ = nullptr;
    int audio_stream_index_ = -1;
    int video_stream_index_ = -1;
};

}  // namespace shatv::media::demux
