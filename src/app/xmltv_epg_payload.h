#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

namespace shatv::app {

std::optional<QString> DecodeXmltvPayload(const QByteArray &payload, const QString &source_name,
                                          QString *error_message = nullptr);

}  // namespace shatv::app
