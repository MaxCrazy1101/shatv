#include "media/ffmpeg_error.h"

extern "C" {
#include <libavutil/error.h>
}

namespace shatv::media {

QString FfmpegErrorString(int error_code) {
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {};
    if (av_strerror(error_code, buffer, sizeof(buffer)) < 0) {
        return QStringLiteral("FFmpeg error %1").arg(error_code);
    }
    return QString::fromUtf8(buffer);
}

}  // namespace shatv::media
