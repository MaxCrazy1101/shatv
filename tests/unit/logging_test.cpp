#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include "app/logging.h"

namespace {

class LoggingTest final : public QObject {
    Q_OBJECT

   private slots:
    void redacts_url_query_values_and_user_info();
    void preserves_local_file_paths_for_diagnostics();
    void rotates_active_log_and_limited_backups();
    void creates_log_file_and_writes_qt_messages();
    void disables_file_logging_when_directory_cannot_be_created();
};

bool WriteTextFile(const QString &path, const QString &text) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    file.write(text.toUtf8());
    return true;
}

QString ReadTextFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

void LoggingTest::redacts_url_query_values_and_user_info() {
    const QString redacted = shatv::app::RedactUrlForLog(
        QUrl(QStringLiteral("https://user:password@example.com/live/index.m3u8?token=secret&quality=hd#frag")));

    QCOMPARE(redacted, QStringLiteral("https://example.com/live/index.m3u8?token=redacted&quality=redacted"));
    QVERIFY(!redacted.contains(QStringLiteral("secret")));
    QVERIFY(!redacted.contains(QStringLiteral("password")));
    QVERIFY(!redacted.contains(QStringLiteral("frag")));
}

void LoggingTest::preserves_local_file_paths_for_diagnostics() {
    const QString local_path = QDir::temp().filePath(QStringLiteral("ShaTV sample.ts"));
    const QString redacted = shatv::app::RedactUrlForLog(QUrl::fromLocalFile(local_path));

    QCOMPARE(redacted, QDir::toNativeSeparators(local_path));
}

void LoggingTest::rotates_active_log_and_limited_backups() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString active_path = temp_dir.filePath(QStringLiteral("shatv.log"));
    QVERIFY(WriteTextFile(active_path, QStringLiteral("active oversized\n")));
    QVERIFY(WriteTextFile(active_path + QStringLiteral(".1"), QStringLiteral("backup one\n")));
    QVERIFY(WriteTextFile(active_path + QStringLiteral(".2"), QStringLiteral("backup two\n")));

    QString error_message;
    QVERIFY2(shatv::app::RotateLogFiles(active_path, 5, 2, &error_message),
             qPrintable(error_message));

    QVERIFY(!QFile::exists(active_path));
    QCOMPARE(ReadTextFile(active_path + QStringLiteral(".1")), QStringLiteral("active oversized\n"));
    QCOMPARE(ReadTextFile(active_path + QStringLiteral(".2")), QStringLiteral("backup one\n"));
    QVERIFY(!ReadTextFile(active_path + QStringLiteral(".2")).contains(QStringLiteral("backup two")));
}

void LoggingTest::creates_log_file_and_writes_qt_messages() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString log_path = temp_dir.filePath(QStringLiteral("shatv.log"));
    QVERIFY(shatv::app::InitializeLogging(shatv::app::LoggingOptions{
        .app_name = QStringLiteral("ShaTVTest"),
        .log_directory = temp_dir.path(),
        .max_file_bytes = 4096,
        .max_backup_files = 1,
    }));
    QVERIFY(shatv::app::LoggingEnabled());
    QCOMPARE(shatv::app::CurrentLogFilePath(), log_path);

    qCInfo(shatv::app::log_app) << "logging test message";
    shatv::app::ShutdownLogging();

    const QString log_text = ReadTextFile(log_path);
    QVERIFY2(log_text.contains(QStringLiteral("INFO shatv.app")),
             qPrintable(log_text));
    QVERIFY2(log_text.contains(QStringLiteral("logging test message")),
             qPrintable(log_text));
}

void LoggingTest::disables_file_logging_when_directory_cannot_be_created() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString file_path = temp_dir.filePath(QStringLiteral("not-a-directory"));
    QVERIFY(WriteTextFile(file_path, QStringLiteral("occupied\n")));

    QVERIFY(!shatv::app::InitializeLogging(shatv::app::LoggingOptions{
        .app_name = QStringLiteral("ShaTVTest"),
        .log_directory = file_path,
        .max_file_bytes = 4096,
        .max_backup_files = 1,
    }));
    QVERIFY(!shatv::app::LoggingEnabled());
    QVERIFY(!QFile::exists(QDir(file_path).filePath(QStringLiteral("shatv.log"))));
    shatv::app::ShutdownLogging();
}

}  // namespace

QTEST_GUILESS_MAIN(LoggingTest)
#include "logging_test.moc"
