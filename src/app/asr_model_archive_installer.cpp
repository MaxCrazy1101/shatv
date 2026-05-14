#include "app/asr_model_archive_installer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <utility>

#if SHATV_HAS_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace shatv::app {

namespace {

QString SafeFileName(QString value) {
    value = value.trimmed();
    QString safe;
    safe.reserve(value.size());
    for (const QChar ch : value) {
        safe.append(ch.isLetterOrNumber() || ch == QLatin1Char('-') || ch == QLatin1Char('_') ||
                            ch == QLatin1Char('.')
                        ? ch
                        : QLatin1Char('_'));
    }
    return safe.isEmpty() ? QStringLiteral("asr-model") : safe;
}

QString NativePath(const QString &path) {
    return QDir::toNativeSeparators(path);
}

AsrModelArchiveInstallResult Failure(const QString &install_dir, const QString &message) {
    return AsrModelArchiveInstallResult{
        .success = false,
        .install_dir = install_dir,
        .error_message = message,
    };
}

#if SHATV_HAS_LIBARCHIVE
bool NormalizeArchivePath(QString raw_path, QString *relative_path, QString *error_message) {
    raw_path = raw_path.trimmed();
    raw_path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (raw_path.isEmpty() || raw_path.startsWith(QLatin1Char('/')) || raw_path.contains(QLatin1Char(':'))) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Unsafe archive entry path: %1").arg(raw_path);
        }
        return false;
    }

    QStringList safe_parts;
    for (const QString &part : raw_path.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
        if (part == QStringLiteral(".") || part.isEmpty()) {
            continue;
        }
        if (part == QStringLiteral("..")) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Unsafe archive entry path: %1").arg(raw_path);
            }
            return false;
        }
        safe_parts.push_back(part);
    }

    if (safe_parts.isEmpty()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Unsafe archive entry path: %1").arg(raw_path);
        }
        return false;
    }

    if (relative_path != nullptr) {
        *relative_path = safe_parts.join(QLatin1Char('/'));
    }
    return true;
}

bool DestinationPathInsideRoot(const QString &root_dir, const QString &destination_path) {
    const QString root = QFileInfo(root_dir).absoluteFilePath();
    const QString destination = QFileInfo(destination_path).absoluteFilePath();
    return destination == root || destination.startsWith(root + QLatin1Char('/'));
}

QString EntryPathUtf8OrLocal8Bit(const char *utf8_path, const char *local_path) {
    if (utf8_path != nullptr) {
        return QString::fromUtf8(utf8_path);
    }
    return local_path == nullptr ? QString{} : QString::fromLocal8Bit(local_path);
}
#endif

bool DirectoryHasRequiredFiles(const QString &directory, const AsrModelFileSet &files, QStringList *missing_files) {
    const QDir dir(directory);
    QStringList missing;
    for (const QString &file_name : {files.encoder_name, files.decoder_name, files.tokens_name}) {
        const QString path = dir.filePath(file_name);
        if (!QFileInfo(path).isFile()) {
            missing.push_back(path);
        }
    }

    if (missing_files != nullptr) {
        *missing_files = missing;
    }
    return missing.isEmpty();
}

QString FindExtractedModelDirectory(const QString &extract_root,
                                    const AsrModelManifest &manifest,
                                    QString *error_message) {
    QStringList missing_files;
    if (DirectoryHasRequiredFiles(extract_root, manifest.files, &missing_files)) {
        return extract_root;
    }

    const QFileInfoList child_dirs = QDir(extract_root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &child_dir : child_dirs) {
        if (DirectoryHasRequiredFiles(child_dir.absoluteFilePath(), manifest.files, nullptr)) {
            return child_dir.absoluteFilePath();
        }
    }

    if (error_message != nullptr) {
        *error_message = QStringLiteral("Extracted ASR model is missing required files: %1")
                             .arg(missing_files.join(QStringLiteral(", ")));
    }
    return {};
}

bool WriteInstallManifest(const QString &model_dir, const AsrModelManifest &manifest, QString *error_message) {
    const QJsonObject files{
        {QStringLiteral("encoder"), manifest.files.encoder_name},
        {QStringLiteral("decoder"), manifest.files.decoder_name},
        {QStringLiteral("tokens"), manifest.files.tokens_name},
    };
    const QJsonObject object{
        {QStringLiteral("id"), manifest.id},
        {QStringLiteral("version"), manifest.version},
        {QStringLiteral("display_name"), manifest.display_name},
        {QStringLiteral("source_url"), manifest.source_url},
        {QStringLiteral("archive_size_bytes"), manifest.archive_size_bytes},
        {QStringLiteral("installed_size_bytes"), manifest.installed_size_bytes},
        {QStringLiteral("archive_sha256"), manifest.archive_sha256},
        {QStringLiteral("license"), manifest.license},
        {QStringLiteral("attribution"), manifest.attribution},
        {QStringLiteral("installed_at_utc"),
         QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("files"), files},
    };

    QFile file(QDir(model_dir).filePath(QStringLiteral("asr_model_manifest.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Failed to write ASR model install manifest: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Failed to write ASR model install manifest: %1").arg(file.errorString());
        }
        return false;
    }
    return true;
}

#if SHATV_HAS_LIBARCHIVE
QString ArchiveError(archive *reader) {
    const char *message = archive_error_string(reader);
    return message == nullptr ? QStringLiteral("unknown libarchive error") : QString::fromLocal8Bit(message);
}

bool ExtractArchiveWithLibArchive(const QString &archive_path,
                                  const QString &destination_root,
                                  QString *error_message) {
    archive *reader = archive_read_new();
    if (reader == nullptr) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Failed to allocate libarchive reader");
        }
        return false;
    }

    archive_read_support_filter_all(reader);
    archive_read_support_format_all(reader);

    const QByteArray native_archive_path = QFile::encodeName(archive_path);
    int result = archive_read_open_filename(reader, native_archive_path.constData(), 1024 * 64);
    if (result != ARCHIVE_OK) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Failed to open ASR model archive: %1").arg(ArchiveError(reader));
        }
        archive_read_free(reader);
        return false;
    }

    archive_entry *entry = nullptr;
    while ((result = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
        const QString raw_entry_path =
            EntryPathUtf8OrLocal8Bit(archive_entry_pathname_utf8(entry), archive_entry_pathname(entry));
        QString relative_path;
        if (!NormalizeArchivePath(raw_entry_path, &relative_path, error_message)) {
            archive_read_free(reader);
            return false;
        }

        const QString destination_path = QDir(destination_root).filePath(relative_path);
        if (!DestinationPathInsideRoot(destination_root, destination_path)) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Unsafe archive entry destination: %1").arg(destination_path);
            }
            archive_read_free(reader);
            return false;
        }

        const auto file_type = archive_entry_filetype(entry);
        if (file_type == AE_IFDIR) {
            if (!QDir().mkpath(destination_path)) {
                if (error_message != nullptr) {
                    *error_message = QStringLiteral("Failed to create ASR model directory: %1")
                                         .arg(NativePath(destination_path));
                }
                archive_read_free(reader);
                return false;
            }
            continue;
        }

        if (file_type != AE_IFREG) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Unsupported ASR model archive entry type: %1")
                                     .arg(raw_entry_path);
            }
            archive_read_free(reader);
            return false;
        }

        const QString parent_dir = QFileInfo(destination_path).absolutePath();
        if (!QDir().mkpath(parent_dir)) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Failed to create ASR model directory: %1")
                                     .arg(NativePath(parent_dir));
            }
            archive_read_free(reader);
            return false;
        }

        QFile output(destination_path);
        if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Failed to extract ASR model file: %1")
                                     .arg(output.errorString());
            }
            archive_read_free(reader);
            return false;
        }

        const void *buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        while ((result = archive_read_data_block(reader, &buffer, &size, &offset)) == ARCHIVE_OK) {
            if (!output.seek(offset)) {
                if (error_message != nullptr) {
                    *error_message = QStringLiteral("Failed to seek ASR model file while extracting");
                }
                archive_read_free(reader);
                return false;
            }
            if (output.write(static_cast<const char *>(buffer), static_cast<qint64>(size)) !=
                static_cast<qint64>(size)) {
                if (error_message != nullptr) {
                    *error_message = QStringLiteral("Failed to write ASR model file while extracting: %1")
                                         .arg(output.errorString());
                }
                archive_read_free(reader);
                return false;
            }
        }

        if (result != ARCHIVE_EOF) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Failed to read ASR model archive data: %1")
                                     .arg(ArchiveError(reader));
            }
            archive_read_free(reader);
            return false;
        }
    }

    if (result != ARCHIVE_EOF) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Failed to read ASR model archive: %1").arg(ArchiveError(reader));
        }
        archive_read_free(reader);
        return false;
    }

    archive_read_free(reader);
    return true;
}
#endif

bool ActivateModelDirectory(const QString &model_dir,
                            const QString &model_root,
                            const AsrModelManifest &manifest,
                            QString *error_message) {
    const QString safe_id = SafeFileName(manifest.id);
    const QDir root_dir(model_root);
    const QString final_dir = root_dir.filePath(manifest.id);
    const QString staging_dir = root_dir.filePath(QStringLiteral(".%1.new").arg(safe_id));
    const QString backup_dir = root_dir.filePath(QStringLiteral(".%1.backup").arg(safe_id));

    if (QFileInfo(final_dir).exists() && !QFileInfo(final_dir).isDir()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("ASR model install path exists and is not a directory: %1")
                                 .arg(NativePath(final_dir));
        }
        return false;
    }

    QDir(staging_dir).removeRecursively();
    QDir(backup_dir).removeRecursively();

    if (!QDir().rename(model_dir, staging_dir)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Failed to stage ASR model directory for activation");
        }
        return false;
    }

    bool final_moved_to_backup = false;
    if (QFileInfo(final_dir).exists()) {
        if (!QDir().rename(final_dir, backup_dir)) {
            QDir().rename(staging_dir, model_dir);
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Failed to back up previous ASR model install directory");
            }
            return false;
        }
        final_moved_to_backup = true;
    }

    if (!QDir().rename(staging_dir, final_dir)) {
        if (final_moved_to_backup) {
            QDir().rename(backup_dir, final_dir);
        }
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Failed to activate ASR model install directory");
        }
        return false;
    }

    if (final_moved_to_backup) {
        QDir(backup_dir).removeRecursively();
    }
    return true;
}

}  // namespace

AsrModelArchiveInstaller::AsrModelArchiveInstaller(QString model_root)
    : model_root_(std::move(model_root)) {}

bool AsrModelArchiveInstaller::Supported() {
    return SHATV_HAS_LIBARCHIVE != 0;
}

AsrModelArchiveInstallResult AsrModelArchiveInstaller::InstallVerifiedArchive(
    const QString &archive_path,
    const AsrModelManifest &manifest) const {
    const QString final_dir = model_root_.isEmpty() ? QString{} : QDir(model_root_).filePath(manifest.id);
    if (!Supported()) {
        return Failure(final_dir, QStringLiteral("ASR model archive extraction requires libarchive support"));
    }
    if (model_root_.trimmed().isEmpty()) {
        return Failure(final_dir, QStringLiteral("ASR model install directory is unavailable"));
    }
    if (!QFileInfo(archive_path).isFile()) {
        return Failure(final_dir, QStringLiteral("ASR model archive is missing: %1").arg(NativePath(archive_path)));
    }
    if (manifest.id.trimmed().isEmpty()) {
        return Failure(final_dir, QStringLiteral("ASR model manifest id is missing"));
    }

    QDir root_dir(model_root_);
    if (!root_dir.mkpath(QStringLiteral("."))) {
        return Failure(final_dir, QStringLiteral("Failed to create ASR model install root: %1")
                                      .arg(NativePath(model_root_)));
    }

    QTemporaryDir temp_dir(root_dir.filePath(QStringLiteral(".extract-XXXXXX")));
    if (!temp_dir.isValid()) {
        return Failure(final_dir, QStringLiteral("Failed to create ASR model extraction directory"));
    }
    const QString extract_root = QDir(temp_dir.path()).filePath(QStringLiteral("contents"));
    if (!QDir().mkpath(extract_root)) {
        return Failure(final_dir, QStringLiteral("Failed to create ASR model extraction directory"));
    }

    QString error_message;
#if SHATV_HAS_LIBARCHIVE
    if (!ExtractArchiveWithLibArchive(archive_path, extract_root, &error_message)) {
        return Failure(final_dir, error_message);
    }
#else
    Q_UNUSED(archive_path);
    return Failure(final_dir, QStringLiteral("ASR model archive extraction requires libarchive support"));
#endif

    const QString extracted_model_dir = FindExtractedModelDirectory(extract_root, manifest, &error_message);
    if (extracted_model_dir.isEmpty()) {
        return Failure(final_dir, error_message);
    }

    if (!WriteInstallManifest(extracted_model_dir, manifest, &error_message)) {
        return Failure(final_dir, error_message);
    }

    if (!ActivateModelDirectory(extracted_model_dir, model_root_, manifest, &error_message)) {
        return Failure(final_dir, error_message);
    }

    return AsrModelArchiveInstallResult{
        .success = true,
        .install_dir = final_dir,
        .error_message = {},
    };
}

}  // namespace shatv::app
