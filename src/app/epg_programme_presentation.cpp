#include "app/epg_programme_presentation.h"

#include <algorithm>

namespace shatv::app {

namespace {

QString BuildTimeText(const QDateTime &start_at, const QDateTime &stop_at) {
    if (!start_at.isValid() || !stop_at.isValid()) {
        return {};
    }

    const QDateTime local_start = start_at.toLocalTime();
    const QDateTime local_stop = stop_at.toLocalTime();
    return QString("%1-%2").arg(local_start.toString("HH:mm"), local_stop.toString("HH:mm"));
}

bool HasValidInterval(const QDateTime &start_at, const QDateTime &stop_at) {
    return start_at.isValid() && stop_at.isValid() && start_at < stop_at;
}

}  // namespace

bool ProgrammePresentation::HasContent() const {
    return !title.isEmpty() || !time_text.isEmpty();
}

bool EpgProgrammePresentation::HasProgrammeInfo() const {
    return current.HasContent() || next.HasContent();
}

ProgrammePresentation BuildProgrammePresentation(const std::optional<XmltvProgramme> &programme, const QDateTime &now,
                                                 bool include_progress) {
    ProgrammePresentation presentation;
    if (!programme.has_value()) {
        return presentation;
    }

    presentation.title = programme->title;
    presentation.time_text = BuildTimeText(programme->start_at, programme->stop_at);

    if (!include_progress || !now.isValid() || !HasValidInterval(programme->start_at, programme->stop_at)) {
        return presentation;
    }

    const qint64 elapsed_ms = programme->start_at.msecsTo(now);
    const qint64 duration_ms = programme->start_at.msecsTo(programme->stop_at);
    presentation.progress = std::clamp(static_cast<double>(elapsed_ms) / static_cast<double>(duration_ms), 0.0, 1.0);
    presentation.progress_available = true;
    return presentation;
}

EpgProgrammePresentation BuildEpgProgrammePresentation(const ChannelEpgNowNext &now_next, const QDateTime &now) {
    return EpgProgrammePresentation{
        .current = BuildProgrammePresentation(now_next.current, now, true),
        .next = BuildProgrammePresentation(now_next.next, now, false),
    };
}

}  // namespace shatv::app
