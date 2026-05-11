#include "app/asr_model_service.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace shatv::app {

namespace {

constexpr char kAsrModelDirEnvName[] = "SHATV_ASR_MODEL_DIR";
constexpr char kAsrEncoderNameEnvName[] = "SHATV_ASR_ENCODER_NAME";
constexpr char kAsrDecoderNameEnvName[] = "SHATV_ASR_DECODER_NAME";
constexpr char kAsrTokensNameEnvName[] = "SHATV_ASR_TOKENS_NAME";

QString EnvironmentValue(const char *name) {
    return qEnvironmentVariable(name).trimmed();
}

QString EnvironmentFileName(const char *name, const QString &default_file_name) {
    const QString value = EnvironmentValue(name);
    return value.isEmpty() ? default_file_name : value;
}

QString RequiredString(const QJsonObject &object, const char *key, QString *error_message) {
    const QString field = QString::fromLatin1(key);
    const QJsonValue value = object.value(field);
    if (!value.isString() || value.toString().trimmed().isEmpty()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("manifest field %1 must be a non-empty string").arg(field);
        }
        return {};
    }
    return value.toString().trimmed();
}

qint64 RequiredNonNegativeInteger(const QJsonObject &object, const char *key, QString *error_message) {
    const QString field = QString::fromLatin1(key);
    const QJsonValue value = object.value(field);
    if (!value.isDouble()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("manifest field %1 must be a non-negative integer").arg(field);
        }
        return -1;
    }

    const double number = value.toDouble(-1.0);
    const auto integer = static_cast<qint64>(number);
    if (number < 0.0 || static_cast<double>(integer) != number) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("manifest field %1 must be a non-negative integer").arg(field);
        }
        return -1;
    }
    return integer;
}

QStringList RequiredModelFiles(const AsrModelFileSet &files) {
    return QStringList{files.encoder_name, files.decoder_name, files.tokens_name};
}

}  // namespace

bool AsrModelStatus::Available() const {
    return status == AsrModelInstallStatus::kInstalled ||
           status == AsrModelInstallStatus::kDeveloperOverride;
}

AsrModelService::AsrModelService(QString model_root, AsrModelManifest manifest)
    : model_root_(std::move(model_root)), manifest_(std::move(manifest)) {}

QString AsrModelService::DefaultModelRoot() {
    const QString app_data_root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(app_data_root).filePath(QStringLiteral("asr-models"));
}

AsrModelManifest AsrModelService::DefaultManifest() {
    return AsrModelManifest{
        .id = QStringLiteral("sherpa-onnx-streaming-paraformer-bilingual-zh-en-int8"),
        .version = QStringLiteral("manual-2026-05-11"),
        .display_name = QStringLiteral("Streaming Paraformer bilingual zh/en int8"),
        .source_url = QStringLiteral("https://github.com/k2-fsa/sherpa-onnx/releases/tag/asr-models"),
        .archive_size_bytes = 0,
        .installed_size_bytes = 0,
        .archive_sha256 = QStringLiteral("5462a1fce42693deae572af1e8c4687124b12aa85fe61ff4d3168bb5280e205f"),
        .license = QStringLiteral("review-required"),
        .attribution = QStringLiteral("csukuangfj/sherpa-onnx-streaming-paraformer-bilingual-zh-en"),
        .files = {},
    };
}

std::optional<AsrModelManifest> AsrModelService::ManifestFromJson(const QByteArray &json, QString *error_message) {
    QJsonParseError parse_error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("invalid ASR model manifest JSON");
        }
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    AsrModelManifest manifest;
    manifest.id = RequiredString(object, "id", error_message);
    if (manifest.id.isEmpty()) {
        return std::nullopt;
    }
    manifest.version = RequiredString(object, "version", error_message);
    if (manifest.version.isEmpty()) {
        return std::nullopt;
    }
    manifest.display_name = RequiredString(object, "display_name", error_message);
    if (manifest.display_name.isEmpty()) {
        return std::nullopt;
    }
    manifest.source_url = RequiredString(object, "source_url", error_message);
    if (manifest.source_url.isEmpty()) {
        return std::nullopt;
    }
    manifest.archive_size_bytes = RequiredNonNegativeInteger(object, "archive_size_bytes", error_message);
    if (manifest.archive_size_bytes < 0) {
        return std::nullopt;
    }
    manifest.installed_size_bytes = RequiredNonNegativeInteger(object, "installed_size_bytes", error_message);
    if (manifest.installed_size_bytes < 0) {
        return std::nullopt;
    }
    manifest.archive_sha256 = RequiredString(object, "archive_sha256", error_message);
    if (manifest.archive_sha256.isEmpty()) {
        return std::nullopt;
    }
    manifest.license = RequiredString(object, "license", error_message);
    if (manifest.license.isEmpty()) {
        return std::nullopt;
    }
    manifest.attribution = RequiredString(object, "attribution", error_message);
    if (manifest.attribution.isEmpty()) {
        return std::nullopt;
    }

    const QJsonValue files_value = object.value(QStringLiteral("files"));
    if (!files_value.isObject()) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("manifest field files must be an object");
        }
        return std::nullopt;
    }
    const QJsonObject files = files_value.toObject();
    manifest.files.encoder_name = RequiredString(files, "encoder", error_message);
    if (manifest.files.encoder_name.isEmpty()) {
        return std::nullopt;
    }
    manifest.files.decoder_name = RequiredString(files, "decoder", error_message);
    if (manifest.files.decoder_name.isEmpty()) {
        return std::nullopt;
    }
    manifest.files.tokens_name = RequiredString(files, "tokens", error_message);
    if (manifest.files.tokens_name.isEmpty()) {
        return std::nullopt;
    }

    return manifest;
}

const AsrModelManifest &AsrModelService::SelectedManifest() const {
    return manifest_;
}

AsrModelFileSet AsrModelService::EffectiveFiles() const {
    return AsrModelFileSet{
        .encoder_name = EnvironmentFileName(kAsrEncoderNameEnvName, manifest_.files.encoder_name),
        .decoder_name = EnvironmentFileName(kAsrDecoderNameEnvName, manifest_.files.decoder_name),
        .tokens_name = EnvironmentFileName(kAsrTokensNameEnvName, manifest_.files.tokens_name),
    };
}

QString AsrModelService::InstallDirectory() const {
    return QDir(model_root_).filePath(manifest_.id);
}

AsrModelStatus AsrModelService::InstalledModelStatus() const {
    const QString developer_model_dir = EnvironmentValue(kAsrModelDirEnvName);
    if (!developer_model_dir.isEmpty()) {
        return StatusForDirectory(developer_model_dir, AsrModelInstallSource::kDeveloperOverride, true);
    }
    return StatusForDirectory(InstallDirectory(), AsrModelInstallSource::kAppManaged, false);
}

AsrModelStatus AsrModelService::StatusForDirectory(const QString &model_dir,
                                                   AsrModelInstallSource source,
                                                   bool directory_required) const {
    AsrModelStatus status;
    status.source = source;
    status.model_dir = model_dir;
    const QFileInfo dir_info(model_dir);
    if (!dir_info.isDir()) {
        status.status = directory_required ? AsrModelInstallStatus::kIncomplete
                                           : AsrModelInstallStatus::kNotInstalled;
        status.message = directory_required
                             ? QStringLiteral("ASR model directory is missing: %1").arg(QDir::toNativeSeparators(model_dir))
                             : QStringLiteral("ASR model is not installed");
        return status;
    }

    const AsrModelFileSet files = EffectiveFiles();
    const QDir dir(model_dir);
    for (const QString &file_name : RequiredModelFiles(files)) {
        const QString path = dir.filePath(file_name);
        if (!QFileInfo(path).isFile()) {
            status.missing_files.push_back(path);
        }
    }

    if (!status.missing_files.isEmpty()) {
        status.status = AsrModelInstallStatus::kIncomplete;
        status.message = QStringLiteral("ASR model is missing required files");
        return status;
    }

    status.status = source == AsrModelInstallSource::kDeveloperOverride
                        ? AsrModelInstallStatus::kDeveloperOverride
                        : AsrModelInstallStatus::kInstalled;
    status.message.clear();
    return status;
}

}  // namespace shatv::app
