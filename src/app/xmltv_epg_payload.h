#pragma once

#include <optional>

#include <QByteArray>
#include <QString>

namespace shatv::app {

std::optional<QString> DecodeXmltvPayload(const QByteArray &payload,
                                          const QString &source_name,
                                          QString *error_message = nullptr);

}  // namespace shatv::app
