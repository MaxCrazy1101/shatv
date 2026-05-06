#include <cstddef>

#include <QtTest>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "app/app_settings.h"
#include "app/open_request.h"

namespace {

using shatv::app::AppSettings;
using shatv::app::OpenRequestKind;
using shatv::app::RecentOpenItem;

template <typename Enum>
int EnumValue(Enum value) {
    return static_cast<int>(value);
}

class AppSettingsRecentTest final : public QObject {
    Q_OBJECT

   private slots:
    void migrates_legacy_kind_entries_to_request_kind();
    void persists_request_kind_target_label_round_trip();
    void rejects_unknown_kind_token_on_load();
};

void AppSettingsRecentTest::migrates_legacy_kind_entries_to_request_kind() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QString config_path = temp_dir.filePath("config.toml");

    QFile config(config_path);
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&config);
    stream << "[[history.recent]]\n"
           << "kind = \"file\"\n"
           << "target = \"/tmp/clip.mp4\"\n"
           << "label = \"clip.mp4\"\n"
           << "[[history.recent]]\n"
           << "kind = \"url\"\n"
           << "target = \"https://example.com/live.m3u8\"\n"
           << "label = \"Live\"\n";
    config.close();

    AppSettings settings(config_path);
    QVERIFY(settings.Load());
    const auto &items = settings.RecentItems();
    QCOMPARE(items.size(), static_cast<std::size_t>(2));
    QCOMPARE(EnumValue(items.at(0).request_kind), EnumValue(OpenRequestKind::kFilePath));
    QCOMPARE(items.at(0).target, QString("/tmp/clip.mp4"));
    QCOMPARE(EnumValue(items.at(1).request_kind), EnumValue(OpenRequestKind::kUrlText));
    QCOMPARE(items.at(1).target, QString("https://example.com/live.m3u8"));
}

void AppSettingsRecentTest::persists_request_kind_target_label_round_trip() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QString config_path = temp_dir.filePath("config.toml");

    {
        AppSettings writer(config_path);
        QVERIFY(writer.Load());
        writer.RememberRecentItem(RecentOpenItem{
            .request_kind = OpenRequestKind::kUrlText,
            .target = "https://example.com/live.m3u8",
            .label = "Live",
        });
        writer.RememberRecentItem(RecentOpenItem{
            .request_kind = OpenRequestKind::kFilePath,
            .target = "/tmp/clip.mp4",
            .label = "clip.mp4",
        });
        QVERIFY(writer.Save());
    }

    AppSettings reader(config_path);
    QVERIFY(reader.Load());
    const auto &items = reader.RecentItems();
    QCOMPARE(items.size(), static_cast<std::size_t>(2));
    QCOMPARE(EnumValue(items.at(0).request_kind), EnumValue(OpenRequestKind::kFilePath));
    QCOMPARE(items.at(0).target, QString("/tmp/clip.mp4"));
    QCOMPARE(items.at(0).label, QString("clip.mp4"));
    QCOMPARE(EnumValue(items.at(1).request_kind), EnumValue(OpenRequestKind::kUrlText));
    QCOMPARE(items.at(1).target, QString("https://example.com/live.m3u8"));
    QCOMPARE(items.at(1).label, QString("Live"));
}

void AppSettingsRecentTest::rejects_unknown_kind_token_on_load() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QString config_path = temp_dir.filePath("config.toml");

    QFile config(config_path);
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream stream(&config);
    stream << "[[history.recent]]\n"
           << "kind = \"bogus\"\n"
           << "target = \"/tmp/clip.mp4\"\n"
           << "label = \"clip.mp4\"\n"
           << "[[history.recent]]\n"
           << "request_kind = \"file_path\"\n"
           << "target = \"/tmp/keep.mp4\"\n"
           << "label = \"keep\"\n";
    config.close();

    AppSettings settings(config_path);
    QVERIFY(settings.Load());
    const auto &items = settings.RecentItems();
    QCOMPARE(items.size(), static_cast<std::size_t>(1));
    QCOMPARE(EnumValue(items.at(0).request_kind), EnumValue(OpenRequestKind::kFilePath));
    QCOMPARE(items.at(0).target, QString("/tmp/keep.mp4"));
}

}  // namespace

QTEST_GUILESS_MAIN(AppSettingsRecentTest)

#include "app_settings_recent_test.moc"
