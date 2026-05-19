#pragma once

#include <QLoggingCategory>
#include <QString>
#include <QUrl>
#include <QtGlobal>

namespace shatv::app {

struct LoggingOptions {
    QString app_name = QStringLiteral("ShaTV");
    QString log_directory;
    qint64 max_file_bytes = 3 * 1024 * 1024;
    int max_backup_files = 3;
};

bool InitializeLogging(const LoggingOptions &options = {});
void ShutdownLogging();

QString CurrentLogFilePath();
QString LogsDirectoryPath();
bool LoggingEnabled();

QString RedactUrlForLog(const QUrl &url);
bool RotateLogFiles(const QString &active_file_path, qint64 max_file_bytes, int max_backup_files,
                    QString *error_message = nullptr);

Q_DECLARE_LOGGING_CATEGORY(log_app)
Q_DECLARE_LOGGING_CATEGORY(log_config)
Q_DECLARE_LOGGING_CATEGORY(log_playback)
Q_DECLARE_LOGGING_CATEGORY(log_ffmpeg)
Q_DECLARE_LOGGING_CATEGORY(log_video)
Q_DECLARE_LOGGING_CATEGORY(log_network)
Q_DECLARE_LOGGING_CATEGORY(log_epg)
Q_DECLARE_LOGGING_CATEGORY(log_qml)

}  // namespace shatv::app
