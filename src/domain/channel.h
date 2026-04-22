#pragma once

#include <QString>
#include <QUrl>
#include <QMetaType>

namespace shatv::domain {

struct Channel {
    QString id;
    QString name;
    QUrl url;
    QString group;
    QString tvg_id;
    QString tvg_name;
};

}  // namespace shatv::domain

Q_DECLARE_METATYPE(shatv::domain::Channel)
