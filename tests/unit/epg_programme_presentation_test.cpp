#include "app/epg_programme_presentation.h"

#include <QTimeZone>
#include <QtTest>

namespace {

using shatv::app::BuildEpgProgrammePresentation;
using shatv::app::BuildProgrammePresentation;
using shatv::app::ChannelEpgNowNext;
using shatv::app::XmltvProgramme;

QDateTime UtcDateTime(int year, int month, int day, int hour, int minute) {
    return QDateTime(QDate(year, month, day), QTime(hour, minute), QTimeZone(QTimeZone::UTC));
}

XmltvProgramme Programme(const QString &title, const QDateTime &start_at, const QDateTime &stop_at) {
    return XmltvProgramme{
        .channel_id = "channel-1",
        .start_at = start_at,
        .stop_at = stop_at,
        .title = title,
    };
}

class EpgProgrammePresentationTest : public QObject {
    Q_OBJECT

   private slots:
    void missing_programmes_return_empty_state();
    void current_programme_progress_is_clamped();
    void invalid_current_interval_hides_progress();
    void next_programme_does_not_expose_progress();
};

void EpgProgrammePresentationTest::missing_programmes_return_empty_state() {
    const shatv::app::EpgProgrammePresentation presentation =
        BuildEpgProgrammePresentation(ChannelEpgNowNext{}, UtcDateTime(2026, 4, 22, 0, 30));

    QVERIFY(!presentation.HasProgrammeInfo());
    QVERIFY(!presentation.current.HasContent());
    QVERIFY(!presentation.next.HasContent());
    QVERIFY(!presentation.current.progress_available);
    QCOMPARE(presentation.current.progress, 0.0);
}

void EpgProgrammePresentationTest::current_programme_progress_is_clamped() {
    const XmltvProgramme programme =
        Programme("Morning News", UtcDateTime(2026, 4, 22, 0, 0), UtcDateTime(2026, 4, 22, 1, 0));

    const shatv::app::ProgrammePresentation before =
        BuildProgrammePresentation(programme, UtcDateTime(2026, 4, 21, 23, 30), true);
    QVERIFY(before.progress_available);
    QCOMPARE(before.progress, 0.0);

    const shatv::app::ProgrammePresentation middle =
        BuildProgrammePresentation(programme, UtcDateTime(2026, 4, 22, 0, 30), true);
    QVERIFY(middle.progress_available);
    QCOMPARE(middle.progress, 0.5);

    const shatv::app::ProgrammePresentation after =
        BuildProgrammePresentation(programme, UtcDateTime(2026, 4, 22, 1, 30), true);
    QVERIFY(after.progress_available);
    QCOMPARE(after.progress, 1.0);
}

void EpgProgrammePresentationTest::invalid_current_interval_hides_progress() {
    const XmltvProgramme programme =
        Programme("Broken Programme", UtcDateTime(2026, 4, 22, 1, 0), UtcDateTime(2026, 4, 22, 1, 0));

    const shatv::app::ProgrammePresentation presentation =
        BuildProgrammePresentation(programme, UtcDateTime(2026, 4, 22, 1, 0), true);

    QVERIFY(presentation.HasContent());
    QVERIFY(!presentation.progress_available);
    QCOMPARE(presentation.progress, 0.0);
}

void EpgProgrammePresentationTest::next_programme_does_not_expose_progress() {
    ChannelEpgNowNext now_next;
    now_next.next = Programme("Later", UtcDateTime(2026, 4, 22, 1, 0), UtcDateTime(2026, 4, 22, 2, 0));

    const shatv::app::EpgProgrammePresentation presentation =
        BuildEpgProgrammePresentation(now_next, UtcDateTime(2026, 4, 22, 0, 30));

    QVERIFY(presentation.HasProgrammeInfo());
    QCOMPARE(presentation.next.title, QString("Later"));
    QVERIFY(!presentation.next.progress_available);
    QCOMPARE(presentation.next.progress, 0.0);
}

}  // namespace

QTEST_GUILESS_MAIN(EpgProgrammePresentationTest)

#include "epg_programme_presentation_test.moc"
