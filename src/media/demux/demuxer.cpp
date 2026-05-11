#include "media/demux/demuxer.h"

#include <chrono>

#include <QByteArray>
#include <QLoggingCategory>
#include <QUrl>

#include "media/ffmpeg_error.h"

extern "C" {
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/rational.h>
}

namespace shatv::media::demux {

namespace {

Q_LOGGING_CATEGORY(log_demuxer, "shatv.ffmpeg.demux")

constexpr int kDefaultRemoteOpenTimeoutMs = 5000;
constexpr int kDefaultRemoteReadTimeoutMs = 8000;
constexpr char kOpenTimeoutEnvName[] = "SHATV_FFMPEG_OPEN_TIMEOUT_MS";
constexpr char kReadTimeoutEnvName[] = "SHATV_FFMPEG_READ_TIMEOUT_MS";

qint64 NowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

bool IsRemoteHttpSource(const domain::MediaSourceDescriptor &source) {
    const QString scheme = source.url.scheme().toLower();
    return scheme == QStringLiteral("http") || scheme == QStringLiteral("https");
}

bool PositiveEnvironmentInt(const char *name, int default_value, int *value, QString *error_message) {
    if (value == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("missing environment output for %1").arg(QString::fromLatin1(name));
        }
        return false;
    }
    if (!qEnvironmentVariableIsSet(name)) {
        *value = default_value;
        return true;
    }

    bool ok = false;
    const int environment_value = qEnvironmentVariableIntValue(name, &ok);
    if (!ok) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("%1 must be an integer").arg(QString::fromLatin1(name));
        }
        return false;
    }
    if (environment_value <= 0) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("%1 must be greater than zero").arg(QString::fromLatin1(name));
        }
        return false;
    }

    *value = environment_value;
    return true;
}

}  // namespace

Demuxer::~Demuxer() {
    Close();
}

bool Demuxer::Open(const domain::MediaSourceDescriptor &source, QString *error_message) {
    return Open(source, {}, error_message);
}

bool Demuxer::Open(const domain::MediaSourceDescriptor &source,
                   DemuxerOpenOptions open_options,
                   QString *error_message) {
    Close();
    abort_requested_ = open_options.abort_requested;
    remote_timeout_config_ = {};
    interrupt_operation_ = static_cast<int>(InterruptOperation::kNone);
    interrupt_reason_ = static_cast<int>(InterruptReason::kNone);
    interrupt_deadline_ms_ = 0;

    if (!source.url.isValid() || source.url.toString().isEmpty()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("invalid media URL");
        }
        return false;
    }

    if (abort_requested_ != nullptr && abort_requested_->load()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("FFmpeg demux open aborted before start");
        }
        return false;
    }

    if (!ConfigureRemoteTimeouts(source, &remote_timeout_config_, error_message)) {
        return false;
    }

    AVDictionary *options = nullptr;
    QByteArray open_timeout_usecs;
    QByteArray read_timeout_usecs;
    // HLS API-style segment compatibility: allow extensionless URLs when the
    // demuxer would otherwise reject them before seeing actual segment data.
    // Only applied for remote sources; local files use the default strict check.
    if (!source.url.isLocalFile()) {
        av_dict_set(&options, "extension_picky", "0", 0);
        av_dict_set(&options, "allowed_segment_extensions", "ALL", 0);
    }
    if (!source.user_agent.isEmpty()) {
        const QByteArray user_agent = source.user_agent.toUtf8();
        av_dict_set(&options, "user_agent", user_agent.constData(), 0);
    }
    if (remote_timeout_config_.enabled) {
        open_timeout_usecs = QByteArray::number(static_cast<qint64>(remote_timeout_config_.open_timeout_ms) * 1000);
        read_timeout_usecs = QByteArray::number(static_cast<qint64>(remote_timeout_config_.read_timeout_ms) * 1000);
        av_dict_set(&options, "timeout", open_timeout_usecs.constData(), 0);
        av_dict_set(&options, "rw_timeout", read_timeout_usecs.constData(), 0);
        qCInfo(log_demuxer).noquote()
            << "FFmpeg remote demux timeouts"
            << "url=" << source.url.toDisplayString(QUrl::RemovePassword | QUrl::RemoveQuery)
            << "openTimeoutMs=" << remote_timeout_config_.open_timeout_ms
            << "readTimeoutMs=" << remote_timeout_config_.read_timeout_ms;
    }

    const QByteArray input_url =
        (source.url.isLocalFile() ? source.url.toLocalFile() : source.url.toString()).toUtf8();
    format_context_ = avformat_alloc_context();
    if (format_context_ == nullptr) {
        av_dict_free(&options);
        if (error_message != nullptr) {
            *error_message = QStringLiteral("avformat_alloc_context failed");
        }
        return false;
    }
    format_context_->interrupt_callback.callback = &Demuxer::InterruptCallback;
    format_context_->interrupt_callback.opaque = this;

    BeginInterruptibleOperation(InterruptOperation::kOpen, remote_timeout_config_.open_timeout_ms);
    const int open_result = avformat_open_input(&format_context_, input_url.constData(), nullptr, &options);
    av_dict_free(&options);
    if (open_result < 0) {
        if (error_message != nullptr) {
            const QString interrupt_error = InterruptErrorMessage();
            *error_message = interrupt_error.isEmpty()
                                 ? QStringLiteral("avformat_open_input failed: %1").arg(FfmpegErrorString(open_result))
                                 : interrupt_error;
        }
        EndInterruptibleOperation();
        Close();
        return false;
    }

    const int stream_info_result = avformat_find_stream_info(format_context_, nullptr);
    if (stream_info_result < 0) {
        if (error_message != nullptr) {
            const QString interrupt_error = InterruptErrorMessage();
            *error_message = interrupt_error.isEmpty()
                                 ? QStringLiteral("avformat_find_stream_info failed: %1")
                                       .arg(FfmpegErrorString(stream_info_result))
                                 : interrupt_error;
        }
        EndInterruptibleOperation();
        Close();
        return false;
    }
    EndInterruptibleOperation();

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
    abort_requested_ = nullptr;
    remote_timeout_config_ = {};
    interrupt_operation_ = static_cast<int>(InterruptOperation::kNone);
    interrupt_reason_ = static_cast<int>(InterruptReason::kNone);
    interrupt_deadline_ms_ = 0;
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
        const int read_result = ReadFrame(packet);
        if (read_result == AVERROR_EOF) {
            return ReadPacketResult::kEndOfFile;
        }
        if (read_result < 0) {
            if (error_message != nullptr) {
                const QString interrupt_error = InterruptErrorMessage();
                *error_message = interrupt_error.isEmpty()
                                     ? QStringLiteral("av_read_frame failed: %1").arg(FfmpegErrorString(read_result))
                                     : interrupt_error;
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
        const int read_result = ReadFrame(packet);
        if (read_result == AVERROR_EOF) {
            return ReadPacketResult::kEndOfFile;
        }
        if (read_result < 0) {
            if (error_message != nullptr) {
                const QString interrupt_error = InterruptErrorMessage();
                *error_message = interrupt_error.isEmpty()
                                     ? QStringLiteral("av_read_frame failed: %1")
                                           .arg(FfmpegErrorString(read_result))
                                     : interrupt_error;
            }
            return ReadPacketResult::kError;
        }

        if (packet->stream_index == stream_index) {
            return ReadPacketResult::kPacket;
        }
    }
}

int Demuxer::InterruptCallback(void *opaque) {
    if (opaque == nullptr) {
        return 0;
    }
    return static_cast<Demuxer *>(opaque)->ShouldInterrupt() ? 1 : 0;
}

bool Demuxer::ConfigureRemoteTimeouts(const domain::MediaSourceDescriptor &source,
                                      RemoteTimeoutConfig *config,
                                      QString *error_message) const {
    if (config == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("missing demux timeout config output");
        }
        return false;
    }
    *config = {};
    if (!IsRemoteHttpSource(source)) {
        return true;
    }

    config->enabled = true;
    return PositiveEnvironmentInt(kOpenTimeoutEnvName,
                                  kDefaultRemoteOpenTimeoutMs,
                                  &config->open_timeout_ms,
                                  error_message) &&
           PositiveEnvironmentInt(kReadTimeoutEnvName,
                                  kDefaultRemoteReadTimeoutMs,
                                  &config->read_timeout_ms,
                                  error_message);
}

void Demuxer::BeginInterruptibleOperation(InterruptOperation operation, int timeout_ms) {
    interrupt_reason_ = static_cast<int>(InterruptReason::kNone);
    interrupt_operation_ = static_cast<int>(operation);
    interrupt_deadline_ms_ = timeout_ms > 0 ? NowMillis() + timeout_ms : 0;
}

void Demuxer::EndInterruptibleOperation() {
    interrupt_operation_ = static_cast<int>(InterruptOperation::kNone);
    interrupt_deadline_ms_ = 0;
}

bool Demuxer::ShouldInterrupt() {
    if (abort_requested_ != nullptr && abort_requested_->load()) {
        interrupt_reason_ = static_cast<int>(InterruptReason::kAbortRequested);
        return true;
    }

    const qint64 deadline_ms = interrupt_deadline_ms_.load();
    if (deadline_ms <= 0 || NowMillis() < deadline_ms) {
        return false;
    }

    const auto operation = static_cast<InterruptOperation>(interrupt_operation_.load());
    if (operation == InterruptOperation::kOpen) {
        interrupt_reason_ = static_cast<int>(InterruptReason::kOpenTimeout);
        return true;
    }
    if (operation == InterruptOperation::kRead) {
        interrupt_reason_ = static_cast<int>(InterruptReason::kReadTimeout);
        return true;
    }
    return false;
}

QString Demuxer::InterruptErrorMessage() const {
    const auto reason = static_cast<InterruptReason>(interrupt_reason_.load());
    switch (reason) {
        case InterruptReason::kNone:
            return {};
        case InterruptReason::kAbortRequested:
            return QStringLiteral("FFmpeg demux aborted by user request");
        case InterruptReason::kOpenTimeout:
            return QStringLiteral("FFmpeg demux open timed out after %1 ms")
                .arg(remote_timeout_config_.open_timeout_ms);
        case InterruptReason::kReadTimeout:
            return QStringLiteral("FFmpeg demux read timed out after %1 ms")
                .arg(remote_timeout_config_.read_timeout_ms);
    }
    return {};
}

int Demuxer::ReadFrame(AVPacket *packet) {
    BeginInterruptibleOperation(InterruptOperation::kRead, remote_timeout_config_.read_timeout_ms);
    const int read_result = av_read_frame(format_context_, packet);
    EndInterruptibleOperation();
    return read_result;
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
