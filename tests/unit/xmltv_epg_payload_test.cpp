#include <QtTest>

#include <zlib.h>

#include "app/xmltv_epg_payload.h"

namespace {

using shatv::app::DecodeXmltvPayload;

QByteArray GzipUtf8(const QByteArray &input) {
    z_stream stream{};
    int init_result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY);
    if (init_result != Z_OK) {
        return {};
    }

    QByteArray output;
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());

    QByteArray chunk(16 * 1024, Qt::Uninitialized);
    int deflate_result = Z_OK;
    while (deflate_result == Z_OK) {
        stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());

        deflate_result = deflate(&stream, Z_FINISH);
        if (deflate_result != Z_OK && deflate_result != Z_STREAM_END) {
            deflateEnd(&stream);
            return {};
        }

        const qsizetype produced = chunk.size() - static_cast<qsizetype>(stream.avail_out);
        if (produced > 0) {
            output.append(chunk.constData(), produced);
        }
    }

    deflateEnd(&stream);
    return output;
}

class XmltvEpgPayloadTest : public QObject {
    Q_OBJECT

   private slots:
    void decodes_plain_xml_payload();
    void decodes_gzip_payload();
    void rejects_invalid_gzip_payload();
};

void XmltvEpgPayloadTest::decodes_plain_xml_payload() {
    const QByteArray xml = "<tv><channel id=\"cctv1\"/></tv>";
    QString error_message;
    const auto decoded = DecodeXmltvPayload(xml, "https://example.com/epg.xml", &error_message);

    QVERIFY2(decoded.has_value(), qPrintable(error_message));
    QCOMPARE(*decoded, QString::fromUtf8(xml));
}

void XmltvEpgPayloadTest::decodes_gzip_payload() {
    const QByteArray xml = "<tv><channel id=\"cctv1\"/></tv>";
    const QByteArray gzipped = GzipUtf8(xml);
    QVERIFY(!gzipped.isEmpty());

    QString error_message;
    const auto decoded = DecodeXmltvPayload(gzipped, "https://example.com/epg.xml.gz", &error_message);

    QVERIFY2(decoded.has_value(), qPrintable(error_message));
    QCOMPARE(*decoded, QString::fromUtf8(xml));
}

void XmltvEpgPayloadTest::rejects_invalid_gzip_payload() {
    const QByteArray invalid_payload = QByteArray::fromHex("1f8b0800000000000003ffff");
    QString error_message;
    const auto decoded = DecodeXmltvPayload(invalid_payload, "https://example.com/epg.xml.gz", &error_message);

    QVERIFY(!decoded.has_value());
    QVERIFY(!error_message.isEmpty());
}

}  // namespace

QTEST_GUILESS_MAIN(XmltvEpgPayloadTest)

#include "xmltv_epg_payload_test.moc"
