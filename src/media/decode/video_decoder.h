#pragma once

#include <vector>

#include <QString>

#include "media/video/video_frame.h"

struct AVCodecContext;
struct AVCodecParameters;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace shatv::media::decode {

// Decodes compressed video packets and normalizes decoded frames to packed
// YUV420P planes for the presenter.
class VideoDecoder final {
   public:
    VideoDecoder() = default;
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder &) = delete;
    VideoDecoder &operator=(const VideoDecoder &) = delete;

    bool Open(const AVCodecParameters *codec_parameters, double stream_time_base_seconds, QString *error_message);
    void Close();

    bool DecodePacket(const AVPacket &packet, std::vector<media::video::VideoFrame> *frames, QString *error_message);
    bool Flush(std::vector<media::video::VideoFrame> *frames, QString *error_message);

   private:
    bool ReceiveFrames(std::vector<media::video::VideoFrame> *frames, QString *error_message);
    bool ConvertFrame(const AVFrame &frame, media::video::VideoFrame *video_frame, QString *error_message);
    void ResetScaler();

    AVCodecContext *codec_context_ = nullptr;
    SwsContext *sws_context_ = nullptr;
    int scaler_width_ = 0;
    int scaler_height_ = 0;
    int scaler_format_ = -1;
    double stream_time_base_seconds_ = 0.0;
};

}  // namespace shatv::media::decode
