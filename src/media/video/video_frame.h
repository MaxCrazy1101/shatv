#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QSize>

namespace shatv::media::video {

struct VideoFrame {
    QSize size;
    qint64 pts_usecs = -1;
    QByteArray y_plane;
    QByteArray u_plane;
    QByteArray v_plane;
};

}  // namespace shatv::media::video

Q_DECLARE_METATYPE(shatv::media::video::VideoFrame)
