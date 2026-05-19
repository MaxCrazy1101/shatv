#include "app/xmltv_epg_payload.h"

#include <zlib.h>

namespace shatv::app {

namespace {

bool LooksLikeGzipPayload(const QByteArray &payload, const QString &source_name) {
    return payload.startsWith(QByteArray::fromHex("1f8b")) || source_name.trimmed().toLower().endsWith(".gz");
}

std::optional<QByteArray> InflateGzipPayload(const QByteArray &payload, QString *error_message) {
    z_stream stream{};
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(payload.constData()));
    stream.avail_in = static_cast<uInt>(payload.size());

    const int init_result = inflateInit2(&stream, MAX_WBITS + 16);
    if (init_result != Z_OK) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("gzip init failed");
        }
        return std::nullopt;
    }

    QByteArray output;
    QByteArray chunk(16 * 1024, Qt::Uninitialized);
    int inflate_result = Z_OK;

    while (inflate_result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());

        inflate_result = inflate(&stream, Z_NO_FLUSH);
        if (inflate_result == Z_BUF_ERROR && stream.avail_in == 0) {
            inflateEnd(&stream);
            if (error_message != nullptr) {
                *error_message = QStringLiteral("gzip payload ended before the end-of-stream marker");
            }
            return std::nullopt;
        }
        if (inflate_result != Z_OK && inflate_result != Z_STREAM_END) {
            inflateEnd(&stream);
            if (error_message != nullptr) {
                *error_message = QStringLiteral("gzip inflate failed");
                if (stream.msg != nullptr) {
                    *error_message += QStringLiteral(": ") + QString::fromUtf8(stream.msg);
                }
            }
            return std::nullopt;
        }

        const qsizetype produced = chunk.size() - static_cast<qsizetype>(stream.avail_out);
        if (produced > 0) {
            output.append(chunk.constData(), produced);
        }
    }

    inflateEnd(&stream);
    return output;
}

}  // namespace

std::optional<QString> DecodeXmltvPayload(const QByteArray &payload, const QString &source_name,
                                          QString *error_message) {
    if (error_message != nullptr) {
        error_message->clear();
    }

    if (payload.isEmpty()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("empty payload");
        }
        return std::nullopt;
    }

    QByteArray xml_bytes = payload;
    if (LooksLikeGzipPayload(payload, source_name)) {
        const std::optional<QByteArray> inflated = InflateGzipPayload(payload, error_message);
        if (!inflated.has_value()) {
            return std::nullopt;
        }
        xml_bytes = *inflated;
    }

    return QString::fromUtf8(xml_bytes);
}

}  // namespace shatv::app
