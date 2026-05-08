#pragma once

#include <optional>

#include <QDateTime>
#include <QString>

#include "app/epg_service.h"
#include "app/xmltv_epg_parser.h"

namespace shatv::app {

struct ProgrammePresentation {
    QString title;
    QString time_text;
    double progress = 0.0;
    bool progress_available = false;

    bool HasContent() const;
};

struct EpgProgrammePresentation {
    ProgrammePresentation current;
    ProgrammePresentation next;

    bool HasProgrammeInfo() const;
};

ProgrammePresentation BuildProgrammePresentation(const std::optional<XmltvProgramme> &programme,
                                                  const QDateTime &now,
                                                  bool include_progress);
EpgProgrammePresentation BuildEpgProgrammePresentation(const ChannelEpgNowNext &now_next, const QDateTime &now);

}  // namespace shatv::app
