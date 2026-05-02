#include "media/decode/video_decoder.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "media/ffmpeg_error.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

namespace shatv::media::decode {

namespace {

constexpr AVPixelFormat kOutputPixelFormat = AV_PIX_FMT_YUV420P;

void CopyPlane(const uint8_t *source,
               int source_stride,
               int width,
               int height,
               QByteArray *destination) {
    destination->resize(width * height);
    for (int row = 0; row < height; ++row) {
        std::memcpy(destination->data() + row * width, source + row * source_stride, static_cast<std::size_t>(width));
    }
}

}  // namespace

VideoDecoder::~VideoDecoder() {
    Close();
}

bool VideoDecoder::Open(const AVCodecParameters *codec_parameters,
                        double stream_time_base_seconds,
                        QString *error_message) {
    Close();
    stream_time_base_seconds_ = stream_time_base_seconds;

    if (codec_parameters == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("missing video codec parameters");
        }
        return false;
    }

    const AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
    if (codec == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("video decoder not found");
        }
        return false;
    }

    codec_context_ = avcodec_alloc_context3(codec);
    if (codec_context_ == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avcodec_alloc_context3 failed");
        }
        return false;
    }

    const int copy_result = avcodec_parameters_to_context(codec_context_, codec_parameters);
    if (copy_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avcodec_parameters_to_context failed: %1")
                                 .arg(FfmpegErrorString(copy_result));
        }
        Close();
        return false;
    }

    const int open_result = avcodec_open2(codec_context_, codec, nullptr);
    if (open_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avcodec_open2 failed: %1").arg(FfmpegErrorString(open_result));
        }
        Close();
        return false;
    }

    return true;
}

void VideoDecoder::Close() {
    ResetScaler();
    if (codec_context_ != nullptr) {
        avcodec_free_context(&codec_context_);
    }
}

bool VideoDecoder::DecodePacket(const AVPacket &packet,
                                std::vector<media::video::VideoFrame> *frames,
                                QString *error_message) {
    if (codec_context_ == nullptr || frames == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("video decoder is not open");
        }
        return false;
    }

    const int send_result = avcodec_send_packet(codec_context_, &packet);
    if (send_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avcodec_send_packet failed: %1")
                                 .arg(FfmpegErrorString(send_result));
        }
        return false;
    }

    return ReceiveFrames(frames, error_message);
}

bool VideoDecoder::Flush(std::vector<media::video::VideoFrame> *frames, QString *error_message) {
    if (codec_context_ == nullptr || frames == nullptr) {
        return true;
    }

    const int send_result = avcodec_send_packet(codec_context_, nullptr);
    if (send_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avcodec_send_packet flush failed: %1")
                                 .arg(FfmpegErrorString(send_result));
        }
        return false;
    }

    return ReceiveFrames(frames, error_message);
}

bool VideoDecoder::ReceiveFrames(std::vector<media::video::VideoFrame> *frames, QString *error_message) {
    while (true) {
        AVFrame *raw_frame = av_frame_alloc();
        if (raw_frame == nullptr) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("av_frame_alloc failed");
            }
            return false;
        }

        const int receive_result = avcodec_receive_frame(codec_context_, raw_frame);
        if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF) {
            av_frame_free(&raw_frame);
            return true;
        }
        if (receive_result < 0) {
            av_frame_free(&raw_frame);
            if (error_message != nullptr) {
                *error_message = QStringLiteral("avcodec_receive_frame failed: %1")
                                     .arg(FfmpegErrorString(receive_result));
            }
            return false;
        }

        media::video::VideoFrame video_frame;
        const bool converted = ConvertFrame(*raw_frame, &video_frame, error_message);
        av_frame_free(&raw_frame);
        if (!converted) {
            return false;
        }
        frames->push_back(std::move(video_frame));
    }
}

bool VideoDecoder::ConvertFrame(const AVFrame &frame,
                                media::video::VideoFrame *video_frame,
                                QString *error_message) {
    if (video_frame == nullptr || frame.width <= 0 || frame.height <= 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("invalid decoded video frame");
        }
        return false;
    }

    const int chroma_width = (frame.width + 1) / 2;
    const int chroma_height = (frame.height + 1) / 2;
    video_frame->size = QSize(frame.width, frame.height);
    if (frame.best_effort_timestamp != AV_NOPTS_VALUE && stream_time_base_seconds_ > 0.0) {
        video_frame->pts_usecs = static_cast<qint64>(frame.best_effort_timestamp * stream_time_base_seconds_ * 1000000.0);
    }
    video_frame->y_plane.resize(frame.width * frame.height);
    video_frame->u_plane.resize(chroma_width * chroma_height);
    video_frame->v_plane.resize(chroma_width * chroma_height);

    if (frame.format == kOutputPixelFormat) {
        CopyPlane(frame.data[0], frame.linesize[0], frame.width, frame.height, &video_frame->y_plane);
        CopyPlane(frame.data[1], frame.linesize[1], chroma_width, chroma_height, &video_frame->u_plane);
        CopyPlane(frame.data[2], frame.linesize[2], chroma_width, chroma_height, &video_frame->v_plane);
        return true;
    }

    if (sws_context_ == nullptr || scaler_width_ != frame.width || scaler_height_ != frame.height ||
        scaler_format_ != frame.format) {
        ResetScaler();
        sws_context_ = sws_getContext(frame.width,
                                      frame.height,
                                      static_cast<AVPixelFormat>(frame.format),
                                      frame.width,
                                      frame.height,
                                      kOutputPixelFormat,
                                      SWS_BILINEAR,
                                      nullptr,
                                      nullptr,
                                      nullptr);
        if (sws_context_ == nullptr) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("sws_getContext failed");
            }
            return false;
        }
        scaler_width_ = frame.width;
        scaler_height_ = frame.height;
        scaler_format_ = frame.format;
    }

    std::array<uint8_t *, 4> destination_data{
        reinterpret_cast<uint8_t *>(video_frame->y_plane.data()),
        reinterpret_cast<uint8_t *>(video_frame->u_plane.data()),
        reinterpret_cast<uint8_t *>(video_frame->v_plane.data()),
        nullptr,
    };
    std::array<int, 4> destination_linesize{
        frame.width,
        chroma_width,
        chroma_width,
        0,
    };

    const int scaled_height =
        sws_scale(sws_context_, frame.data, frame.linesize, 0, frame.height, destination_data.data(), destination_linesize.data());
    if (scaled_height != frame.height) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("sws_scale failed");
        }
        return false;
    }

    return true;
}

void VideoDecoder::ResetScaler() {
    if (sws_context_ != nullptr) {
        sws_freeContext(sws_context_);
        sws_context_ = nullptr;
    }
    scaler_width_ = 0;
    scaler_height_ = 0;
    scaler_format_ = -1;
}

}  // namespace shatv::media::decode
