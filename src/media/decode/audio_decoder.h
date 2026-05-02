#pragma once

#include <memory>
#include <vector>

#include <QString>

struct AVCodecContext;
struct AVCodecParameters;
struct AVFrame;
struct AVPacket;

namespace shatv::media::decode {

struct AvFrameDeleter {
    void operator()(AVFrame *frame) const;
};

using AvFramePtr = std::unique_ptr<AVFrame, AvFrameDeleter>;

// Decodes compressed audio packets into AVFrame objects.
class AudioDecoder final {
   public:
    AudioDecoder() = default;
    ~AudioDecoder();

    AudioDecoder(const AudioDecoder &) = delete;
    AudioDecoder &operator=(const AudioDecoder &) = delete;

    bool Open(const AVCodecParameters *codec_parameters, QString *error_message);
    void Close();

    bool DecodePacket(const AVPacket &packet, std::vector<AvFramePtr> *frames, QString *error_message);
    bool Flush(std::vector<AvFramePtr> *frames, QString *error_message);

   private:
    bool ReceiveFrames(std::vector<AvFramePtr> *frames, QString *error_message);

    AVCodecContext *codec_context_ = nullptr;
};

}  // namespace shatv::media::decode
