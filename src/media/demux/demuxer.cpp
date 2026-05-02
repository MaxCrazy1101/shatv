#include "media/demux/demuxer.h"

#include <QByteArray>

#include "media/ffmpeg_error.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/rational.h>
}

namespace shatv::media::demux {

Demuxer::~Demuxer() {
    Close();
}

bool Demuxer::Open(const domain::MediaSourceDescriptor &source, QString *error_message) {
    Close();

    if (!source.url.isValid() || source.url.toString().isEmpty()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("invalid media URL");
        }
        return false;
    }

    AVDictionary *options = nullptr;
    if (!source.user_agent.isEmpty()) {
        const QByteArray user_agent = source.user_agent.toUtf8();
        av_dict_set(&options, "user_agent", user_agent.constData(), 0);
    }

    const QByteArray input_url =
        (source.url.isLocalFile() ? source.url.toLocalFile() : source.url.toString()).toUtf8();
    const int open_result = avformat_open_input(&format_context_, input_url.constData(), nullptr, &options);
    av_dict_free(&options);
    if (open_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avformat_open_input failed: %1").arg(FfmpegErrorString(open_result));
        }
        Close();
        return false;
    }

    const int stream_info_result = avformat_find_stream_info(format_context_, nullptr);
    if (stream_info_result < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avformat_find_stream_info failed: %1")
                                 .arg(FfmpegErrorString(stream_info_result));
        }
        Close();
        return false;
    }

    audio_stream_index_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    video_stream_index_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (audio_stream_index_ < 0 && video_stream_index_ < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("no audio or video stream found");
        }
        Close();
        return false;
    }

    return true;
}

void Demuxer::Close() {
    if (format_context_ != nullptr) {
        avformat_close_input(&format_context_);
    }
    audio_stream_index_ = -1;
    video_stream_index_ = -1;
}

ReadPacketResult Demuxer::ReadNextAudioPacket(AVPacket *packet, QString *error_message) {
    if (audio_stream_index_ < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("no audio stream found");
        }
        return ReadPacketResult::kError;
    }

    return ReadNextPacketForStream(audio_stream_index_, packet, error_message);
}

ReadPacketResult Demuxer::ReadNextVideoPacket(AVPacket *packet, QString *error_message) {
    if (video_stream_index_ < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("no video stream found");
        }
        return ReadPacketResult::kError;
    }

    return ReadNextPacketForStream(video_stream_index_, packet, error_message);
}

ReadPacketResult Demuxer::ReadNextMediaPacket(AVPacket *packet, QString *error_message) {
    if (format_context_ == nullptr || packet == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("demuxer is not open");
        }
        return ReadPacketResult::kError;
    }

    while (true) {
        av_packet_unref(packet);
        const int read_result = av_read_frame(format_context_, packet);
        if (read_result == AVERROR_EOF) {
            return ReadPacketResult::kEndOfFile;
        }
        if (read_result < 0) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("av_read_frame failed: %1").arg(FfmpegErrorString(read_result));
            }
            return ReadPacketResult::kError;
        }

        if (IsAudioPacket(*packet) || IsVideoPacket(*packet)) {
            return ReadPacketResult::kPacket;
        }
    }
}

ReadPacketResult Demuxer::ReadNextPacketForStream(int stream_index, AVPacket *packet, QString *error_message) {
    if (format_context_ == nullptr || packet == nullptr || stream_index < 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("demuxer is not open");
        }
        return ReadPacketResult::kError;
    }

    while (true) {
        av_packet_unref(packet);
        const int read_result = av_read_frame(format_context_, packet);
        if (read_result == AVERROR_EOF) {
            return ReadPacketResult::kEndOfFile;
        }
        if (read_result < 0) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("av_read_frame failed: %1")
                                     .arg(FfmpegErrorString(read_result));
            }
            return ReadPacketResult::kError;
        }

        if (packet->stream_index == stream_index) {
            return ReadPacketResult::kPacket;
        }
    }
}

const AVCodecParameters *Demuxer::AudioCodecParameters() const {
    if (format_context_ == nullptr || audio_stream_index_ < 0 ||
        audio_stream_index_ >= static_cast<int>(format_context_->nb_streams)) {
        return nullptr;
    }

    return format_context_->streams[audio_stream_index_]->codecpar;
}

const AVCodecParameters *Demuxer::VideoCodecParameters() const {
    if (format_context_ == nullptr || video_stream_index_ < 0 ||
        video_stream_index_ >= static_cast<int>(format_context_->nb_streams)) {
        return nullptr;
    }

    return format_context_->streams[video_stream_index_]->codecpar;
}

double Demuxer::AudioTimeBaseSeconds() const {
    if (format_context_ == nullptr || audio_stream_index_ < 0 ||
        audio_stream_index_ >= static_cast<int>(format_context_->nb_streams)) {
        return 0.0;
    }

    return av_q2d(format_context_->streams[audio_stream_index_]->time_base);
}

double Demuxer::VideoTimeBaseSeconds() const {
    if (format_context_ == nullptr || video_stream_index_ < 0 ||
        video_stream_index_ >= static_cast<int>(format_context_->nb_streams)) {
        return 0.0;
    }

    return av_q2d(format_context_->streams[video_stream_index_]->time_base);
}

int Demuxer::AudioStreamIndex() const {
    return audio_stream_index_;
}

int Demuxer::VideoStreamIndex() const {
    return video_stream_index_;
}

bool Demuxer::HasAudioStream() const {
    return audio_stream_index_ >= 0;
}

bool Demuxer::HasVideoStream() const {
    return video_stream_index_ >= 0;
}

bool Demuxer::IsAudioPacket(const AVPacket &packet) const {
    return audio_stream_index_ >= 0 && packet.stream_index == audio_stream_index_;
}

bool Demuxer::IsVideoPacket(const AVPacket &packet) const {
    return video_stream_index_ >= 0 && packet.stream_index == video_stream_index_;
}

}  // namespace shatv::media::demux
