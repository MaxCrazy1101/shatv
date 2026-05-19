#include "media/decode/audio_decoder.h"

#include "media/ffmpeg_error.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

namespace shatv::media::decode {

void AvFrameDeleter::operator()(AVFrame *frame) const {
    av_frame_free(&frame);
}

AudioDecoder::~AudioDecoder() {
    Close();
}

bool AudioDecoder::Open(const AVCodecParameters *codec_parameters, QString *error_message) {
    Close();

    if (codec_parameters == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("missing audio codec parameters");
        }
        return false;
    }

    const AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
    if (codec == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("audio decoder not found");
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
            *error_message =
                QStringLiteral("avcodec_parameters_to_context failed: %1").arg(FfmpegErrorString(copy_result));
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

void AudioDecoder::Close() {
    if (codec_context_ != nullptr) {
        avcodec_free_context(&codec_context_);
    }
}

bool AudioDecoder::DecodePacket(const AVPacket &packet, std::vector<AvFramePtr> *frames, QString *error_message) {
    if (codec_context_ == nullptr || frames == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("audio decoder is not open");
        }
        return false;
    }

    const int send_result = avcodec_send_packet(codec_context_, &packet);
    if (send_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avcodec_send_packet failed: %1").arg(FfmpegErrorString(send_result));
        }
        return false;
    }

    return ReceiveFrames(frames, error_message);
}

bool AudioDecoder::Flush(std::vector<AvFramePtr> *frames, QString *error_message) {
    if (codec_context_ == nullptr || frames == nullptr) {
        return true;
    }

    const int send_result = avcodec_send_packet(codec_context_, nullptr);
    if (send_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avcodec_send_packet flush failed: %1").arg(FfmpegErrorString(send_result));
        }
        return false;
    }

    return ReceiveFrames(frames, error_message);
}

bool AudioDecoder::ReceiveFrames(std::vector<AvFramePtr> *frames, QString *error_message) {
    while (true) {
        AvFramePtr frame(av_frame_alloc());
        if (frame == nullptr) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("av_frame_alloc failed");
            }
            return false;
        }

        const int receive_result = avcodec_receive_frame(codec_context_, frame.get());
        if (receive_result == AVERROR(EAGAIN) || receive_result == AVERROR_EOF) {
            return true;
        }
        if (receive_result < 0) {
            if (error_message != nullptr) {
                *error_message =
                    QStringLiteral("avcodec_receive_frame failed: %1").arg(FfmpegErrorString(receive_result));
            }
            return false;
        }

        frames->push_back(std::move(frame));
    }
}

}  // namespace shatv::media::decode
