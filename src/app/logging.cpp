#include "app/logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>
#include <QUrlQuery>
#include <cstdio>

namespace shatv::app {

Q_LOGGING_CATEGORY(log_app, "shatv.app")
Q_LOGGING_CATEGORY(log_config, "shatv.config")
Q_LOGGING_CATEGORY(log_playback, "shatv.playback")
Q_LOGGING_CATEGORY(log_ffmpeg, "shatv.ffmpeg")
Q_LOGGING_CATEGORY(log_video, "shatv.video")
Q_LOGGING_CATEGORY(log_network, "shatv.network")
Q_LOGGING_CATEGORY(log_epg, "shatv.epg")
Q_LOGGING_CATEGORY(log_qml, "shatv.qml")

namespace {

constexpr char kLogFileName[] = "shatv.log";
constexpr char kRedactedValue[] = "redacted";

struct LoggingState {
    QMutex mutex;
    QFile file;
    QString logs_directory;
    QString log_file_path;
    qint64 max_file_bytes = 3 * 1024 * 1024;
    int max_backup_files = 3;
    bool file_logging_enabled = false;
    bool handler_installed = false;
    QtMessageHandler previous_handler = nullptr;
};

LoggingState &State() {
    static LoggingState state;
    return state;
}

QString SeverityName(QtMsgType type) {
    switch (type) {
        case QtDebugMsg:
            return QStringLiteral("DEBUG");
        case QtInfoMsg:
            return QStringLiteral("INFO");
        case QtWarningMsg:
            return QStringLiteral("WARN");
        case QtCriticalMsg:
            return QStringLiteral("ERROR");
        case QtFatalMsg:
            return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}

void WriteStderrLine(const QString &line) {
    const QByteArray bytes = line.toLocal8Bit();
    std::fwrite(bytes.constData(), 1, static_cast<std::size_t>(bytes.size()), stderr);
    std::fwrite("\n", 1, 1, stderr);
    std::fflush(stderr);
}

QString FormatThreadId() {
    QString result;
    QTextStream stream(&result);
    stream << Qt::hex << reinterpret_cast<quintptr>(QThread::currentThreadId());
    return result;
}

QString FormatMessageLine(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    QString line;
    QTextStream stream(&line);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ' ' << SeverityName(type) << ' '
           << (context.category != nullptr ? context.category : "default") << " [" << FormatThreadId() << "] "
           << message;
#ifndef NDEBUG
    if (context.file != nullptr && context.line > 0) {
        stream << " (" << context.file << ':' << context.line << ')';
    }
#endif
    return line;
}

QString BackupPath(const QString &active_file_path, int index) {
    return QStringLiteral("%1.%2").arg(active_file_path).arg(index);
}

bool OpenLogFileLocked(LoggingState *state, QString *error_message) {
    state->file.setFileName(state->log_file_path);
    if (!state->file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (error_message != nullptr) {
            *error_message = state->file.errorString();
        }
        state->file_logging_enabled = false;
        return false;
    }

    state->file_logging_enabled = true;
    return true;
}

void DisableFileLoggingLocked(LoggingState *state, const QString &reason) {
    state->file_logging_enabled = false;
    if (state->file.isOpen()) {
        state->file.close();
    }
    WriteStderrLine(QStringLiteral("ShaTV logging warning: file logging disabled: %1").arg(reason));
}

void MessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message) {
    const QString line = FormatMessageLine(type, context, message);
    WriteStderrLine(line);

    LoggingState &state = State();
    QMutexLocker locker(&state.mutex);
    if (!state.file_logging_enabled) {
        if (type == QtFatalMsg) {
            std::abort();
        }
        return;
    }

    const QByteArray line_bytes = line.toUtf8();
    if (state.max_file_bytes > 0 && state.file.size() + line_bytes.size() + 1 > state.max_file_bytes) {
        state.file.close();
        QString rotate_error;
        if (!RotateLogFiles(state.log_file_path, state.max_file_bytes, state.max_backup_files, &rotate_error)) {
            DisableFileLoggingLocked(&state, QStringLiteral("rotation failed: %1").arg(rotate_error));
            if (type == QtFatalMsg) {
                std::abort();
            }
            return;
        }

        QString open_error;
        if (!OpenLogFileLocked(&state, &open_error)) {
            DisableFileLoggingLocked(&state, QStringLiteral("reopen failed: %1").arg(open_error));
            if (type == QtFatalMsg) {
                std::abort();
            }
            return;
        }
    }

    const qint64 message_bytes_written = state.file.write(line_bytes);
    const qint64 newline_bytes_written = state.file.write("\n");
    if (message_bytes_written != line_bytes.size() || newline_bytes_written != 1 || !state.file.flush()) {
        DisableFileLoggingLocked(&state, QStringLiteral("write failed: %1").arg(state.file.errorString()));
    }

    if (type == QtFatalMsg) {
        std::abort();
    }
}

}  // namespace

bool InitializeLogging(const LoggingOptions &options) {
    LoggingState &state = State();
    QMutexLocker locker(&state.mutex);

    if (state.file.isOpen()) {
        state.file.flush();
        state.file.close();
    }
    state.file_logging_enabled = false;
    state.max_file_bytes = options.max_file_bytes;
    state.max_backup_files = options.max_backup_files;
    const QString standard_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    state.logs_directory = options.log_directory.isEmpty() ? standard_directory : options.log_directory;
    state.log_file_path = state.logs_directory.isEmpty()
                              ? QString()
                              : QDir(state.logs_directory).filePath(QString::fromLatin1(kLogFileName));

    QString setup_warning;
    if (state.logs_directory.isEmpty()) {
        setup_warning = QStringLiteral("Qt did not return a writable AppDataLocation");
    } else {
        QDir logs_dir(state.logs_directory);
        if (!logs_dir.exists() && !logs_dir.mkpath(QStringLiteral("."))) {
            setup_warning = QStringLiteral("failed to create %1").arg(QDir::toNativeSeparators(state.logs_directory));
        }
    }

    if (setup_warning.isEmpty()) {
        QString rotate_error;
        if (!RotateLogFiles(state.log_file_path, state.max_file_bytes, state.max_backup_files, &rotate_error)) {
            setup_warning = QStringLiteral("initial rotation failed: %1").arg(rotate_error);
        }
    }

    if (setup_warning.isEmpty()) {
        QString open_error;
        if (!OpenLogFileLocked(&state, &open_error)) {
            setup_warning =
                QStringLiteral("failed to open %1: %2").arg(QDir::toNativeSeparators(state.log_file_path), open_error);
        }
    }

    if (!state.handler_installed) {
        state.previous_handler = qInstallMessageHandler(MessageHandler);
        state.handler_installed = true;
    }

    if (!setup_warning.isEmpty()) {
        state.file_logging_enabled = false;
        WriteStderrLine(QStringLiteral("ShaTV logging warning: %1; continuing with stderr logging").arg(setup_warning));
    }

    Q_UNUSED(options.app_name);
    return state.file_logging_enabled;
}

void ShutdownLogging() {
    LoggingState &state = State();
    QMutexLocker locker(&state.mutex);
    if (state.handler_installed) {
        qInstallMessageHandler(state.previous_handler);
        state.previous_handler = nullptr;
        state.handler_installed = false;
    }
    if (state.file.isOpen()) {
        state.file.flush();
        state.file.close();
    }
    state.file_logging_enabled = false;
}

QString CurrentLogFilePath() {
    LoggingState &state = State();
    QMutexLocker locker(&state.mutex);
    return state.log_file_path;
}

QString LogsDirectoryPath() {
    LoggingState &state = State();
    QMutexLocker locker(&state.mutex);
    return state.logs_directory;
}

bool LoggingEnabled() {
    LoggingState &state = State();
    QMutexLocker locker(&state.mutex);
    return state.file_logging_enabled;
}

QString RedactUrlForLog(const QUrl &url) {
    if (!url.isValid() || url.isEmpty()) {
        return QStringLiteral("<invalid-url>");
    }

    if (url.isLocalFile()) {
        return QDir::toNativeSeparators(url.toLocalFile());
    }

    QUrl redacted(url);
    redacted.setUserName(QString());
    redacted.setPassword(QString());
    redacted.setFragment(QString());

    QUrlQuery query(redacted);
    if (!query.isEmpty()) {
        QUrlQuery redacted_query;
        for (const auto &item : query.queryItems(QUrl::FullyDecoded)) {
            redacted_query.addQueryItem(item.first, QString::fromLatin1(kRedactedValue));
        }
        redacted.setQuery(redacted_query);
    }

    return redacted.toString(QUrl::RemoveUserInfo | QUrl::RemoveFragment);
}

bool RotateLogFiles(const QString &active_file_path, qint64 max_file_bytes, int max_backup_files,
                    QString *error_message) {
    if (active_file_path.isEmpty() || max_file_bytes <= 0 || max_backup_files <= 0) {
        return true;
    }

    const QFileInfo active_info(active_file_path);
    if (!active_info.exists() || active_info.size() <= max_file_bytes) {
        return true;
    }

    const QString oldest_backup = BackupPath(active_file_path, max_backup_files);
    if (QFile::exists(oldest_backup) && !QFile::remove(oldest_backup)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("failed to remove %1").arg(QDir::toNativeSeparators(oldest_backup));
        }
        return false;
    }

    for (int index = max_backup_files - 1; index >= 1; --index) {
        const QString source = BackupPath(active_file_path, index);
        if (!QFile::exists(source)) {
            continue;
        }

        const QString target = BackupPath(active_file_path, index + 1);
        if (QFile::exists(target) && !QFile::remove(target)) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("failed to replace %1").arg(QDir::toNativeSeparators(target));
            }
            return false;
        }
        if (!QFile::rename(source, target)) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("failed to rename %1").arg(QDir::toNativeSeparators(source));
            }
            return false;
        }
    }

    const QString first_backup = BackupPath(active_file_path, 1);
    if (QFile::exists(first_backup) && !QFile::remove(first_backup)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("failed to replace %1").arg(QDir::toNativeSeparators(first_backup));
        }
        return false;
    }
    if (!QFile::rename(active_file_path, first_backup)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("failed to rotate %1").arg(QDir::toNativeSeparators(active_file_path));
        }
        return false;
    }

    return true;
}

}  // namespace shatv::app
