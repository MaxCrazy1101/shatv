#include "app/asr_model_service.h"

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QUrl>

#include <utility>

namespace shatv::app {

namespace {

constexpr char kAsrModelDirEnvName[] = "SHATV_ASR_MODEL_DIR";
constexpr char kAsrEncoderNameEnvName[] = "SHATV_ASR_ENCODER_NAME";
constexpr char kAsrDecoderNameEnvName[] = "SHATV_ASR_DECODER_NAME";
constexpr char kAsrTokensNameEnvName[] = "SHATV_ASR_TOKENS_NAME";
constexpr int kNetworkTransferTimeoutMillis = 30000;

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

QString ArchiveSuffixFromUrl(const QString &source_url) {
    const QString file_name = QFileInfo(QUrl(source_url).path()).fileName().toLower();
    if (file_name.endsWith(QStringLiteral(".tar.bz2"))) {
        return QStringLiteral(".tar.bz2");
    }
    if (file_name.endsWith(QStringLiteral(".zip"))) {
        return QStringLiteral(".zip");
    }
    return QStringLiteral(".archive");
}

bool FileSha256(const QString &path, QString *sha256, QString *error_message) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("Failed to read ASR model archive: %1").arg(file.errorString());
        }
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
            if (error_message != nullptr) {
                *error_message = QStringLiteral("Failed to read ASR model archive: %1").arg(file.errorString());
            }
            return false;
        }
        hash.addData(chunk);
    }

    if (sha256 != nullptr) {
        *sha256 = QString::fromLatin1(hash.result().toHex());
    }
    return true;
}

bool Sha256Matches(const QString &path, const QString &expected_sha256, QString *actual_sha256, QString *error_message) {
    QString actual;
    if (!FileSha256(path, &actual, error_message)) {
        return false;
    }
    if (actual_sha256 != nullptr) {
        *actual_sha256 = actual;
    }
    return actual.compare(expected_sha256.trimmed(), Qt::CaseInsensitive) == 0;
}

}  // namespace

bool IsSafeAsrModelId(const QString &id) {
    const QString value = id.trimmed();
    if (value.isEmpty() || value == QStringLiteral(".") || value == QStringLiteral("..")) {
        return false;
    }
    for (const QChar ch : value) {
        const ushort code = ch.unicode();
        const bool ascii_letter = (code >= 'A' && code <= 'Z') || (code >= 'a' && code <= 'z');
        const bool ascii_digit = code >= '0' && code <= '9';
        const bool safe_punctuation = ch == QLatin1Char('-') || ch == QLatin1Char('_') || ch == QLatin1Char('.');
        if (!ascii_letter && !ascii_digit && !safe_punctuation) {
            return false;
        }
    }
    return true;
}

bool AsrModelStatus::Available() const {
    return status == AsrModelInstallStatus::kInstalled ||
           status == AsrModelInstallStatus::kDeveloperOverride;
}

AsrModelService::AsrModelService(QString model_root, AsrModelManifest manifest)
    : model_root_(std::move(model_root)), manifest_(std::move(manifest)) {}

QString AsrModelService::DefaultModelRoot() {
    const QString app_local_data_root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (app_local_data_root.isEmpty()) {
        return {};
    }
    return QDir(app_local_data_root).filePath(QStringLiteral("asr-models"));
}

QString AsrModelService::DefaultArchiveCacheRoot() {
    const QString cache_root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cache_root.isEmpty()) {
        return {};
    }
    return QDir(cache_root).filePath(QStringLiteral("asr-model-archives"));
}

AsrModelManifest AsrModelService::DefaultManifest() {
    return AsrModelManifest{
        .id = QStringLiteral("sherpa-onnx-streaming-paraformer-bilingual-zh-en-int8"),
        .version = QStringLiteral("manual-2026-05-11"),
        .display_name = QStringLiteral("Streaming Paraformer bilingual zh/en int8"),
        .source_url = QStringLiteral("https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
                                     "sherpa-onnx-streaming-paraformer-bilingual-zh-en.tar.bz2"),
        .archive_size_bytes = 1047319737,
        .installed_size_bytes = 0,
        .archive_sha256 = QStringLiteral("5462a1fce42693deae572af1e8c4687124b12aa85fe61ff4d3168bb5280e205f"),
        .license = QStringLiteral("review-required"),
        .attribution = QStringLiteral("csukuangfj/sherpa-onnx-streaming-paraformer-bilingual-zh-en"),
        .files = AsrModelFileSet{
            .encoder_name = QStringLiteral("encoder.int8.onnx"),
            .decoder_name = QStringLiteral("decoder.int8.onnx"),
            .tokens_name = QStringLiteral("tokens.txt"),
        },
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
    if (!IsSafeAsrModelId(manifest.id)) {
        if (error_message != nullptr) {
            *error_message = QStringLiteral("manifest field id contains unsafe path characters");
        }
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
    if (model_root_.isEmpty() || !IsSafeAsrModelId(manifest_.id)) {
        return {};
    }
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
    if (model_dir.trimmed().isEmpty()) {
        status.status = AsrModelInstallStatus::kIncomplete;
        status.message = QStringLiteral("ASR model directory is unavailable");
        return status;
    }

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

AsrModelArchiveDownloader::AsrModelArchiveDownloader(QNetworkAccessManager *network_manager,
                                                     QString archive_cache_root,
                                                     QObject *parent)
    : QObject(parent),
      network_manager_(network_manager),
      archive_cache_root_(std::move(archive_cache_root)) {}

AsrModelArchiveDownloader::~AsrModelArchiveDownloader() {
    if (reply_ == nullptr) {
        return;
    }

    reply_->disconnect(this);
    cancel_requested_ = true;
    reply_->abort();
    CleanupPartFile();
    reply_->deleteLater();
    reply_ = nullptr;
}

QString AsrModelArchiveDownloader::ArchivePath(const AsrModelManifest &manifest) const {
    if (archive_cache_root_.trimmed().isEmpty()) {
        return {};
    }

    const QString checksum_suffix = manifest.archive_sha256.trimmed().isEmpty()
                                        ? QStringLiteral("unverified")
                                        : manifest.archive_sha256.trimmed().toLower();
    const QString archive_file_name = QStringLiteral("%1-%2%3")
                                          .arg(SafeFileName(manifest.id), SafeFileName(checksum_suffix),
                                               ArchiveSuffixFromUrl(manifest.source_url));
    return QDir(archive_cache_root_).filePath(archive_file_name);
}

void AsrModelArchiveDownloader::Start(const AsrModelManifest &manifest) {
    if (reply_ != nullptr) {
        AsrModelArchiveDownloadResult result;
        result.success = false;
        result.archive_path = ArchivePath(manifest);
        result.error_message = QStringLiteral("ASR model archive download is already running");
        emit Finished(result);
        return;
    }

    archive_path_ = ArchivePath(manifest);
    part_path_ = archive_path_.isEmpty() ? QString{} : archive_path_ + QStringLiteral(".part");
    bytes_received_ = 0;
    bytes_total_ = -1;
    cancel_requested_ = false;
    hash_.reset();

    if (network_manager_ == nullptr) {
        FinishWithFailure(QStringLiteral("ASR model archive downloader has no network manager"));
        return;
    }
    if (archive_path_.isEmpty()) {
        FinishWithFailure(QStringLiteral("ASR model archive cache directory is unavailable"));
        return;
    }
    if (manifest.archive_sha256.trimmed().isEmpty()) {
        FinishWithFailure(QStringLiteral("ASR model archive checksum is missing"));
        return;
    }

    const QFileInfo existing_archive(archive_path_);
    if (existing_archive.exists() && !existing_archive.isFile()) {
        FinishWithFailure(QStringLiteral("ASR model archive cache path exists and is not a file: %1")
                              .arg(QDir::toNativeSeparators(archive_path_)));
        return;
    }
    if (existing_archive.isFile()) {
        QString actual_sha256;
        QString hash_error;
        if (Sha256Matches(archive_path_, manifest.archive_sha256, &actual_sha256, &hash_error)) {
            bytes_received_ = existing_archive.size();
            bytes_total_ = existing_archive.size();
            FinishWithSuccess();
            return;
        }
        if (!hash_error.isEmpty()) {
            FinishWithFailure(hash_error);
            return;
        }
    }

    const QUrl source_url(manifest.source_url);
    if (!source_url.isValid() || source_url.isEmpty()) {
        FinishWithFailure(QStringLiteral("ASR model archive source URL is invalid"));
        return;
    }

    QDir cache_dir(archive_cache_root_);
    if (!cache_dir.mkpath(QStringLiteral("."))) {
        FinishWithFailure(QStringLiteral("Failed to create ASR model archive cache directory: %1")
                              .arg(QDir::toNativeSeparators(archive_cache_root_)));
        return;
    }

    QFile::remove(part_path_);
    output_file_.setFileName(part_path_);
    if (!output_file_.open(QIODevice::WriteOnly)) {
        FinishWithFailure(QStringLiteral("Failed to write ASR model archive cache: %1").arg(output_file_.errorString()));
        return;
    }

    QNetworkRequest request(source_url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(kNetworkTransferTimeoutMillis);
    reply_ = network_manager_->get(request);
    connect(reply_, &QNetworkReply::readyRead, this, &AsrModelArchiveDownloader::OnReadyRead);
    connect(reply_, &QNetworkReply::downloadProgress, this, &AsrModelArchiveDownloader::OnDownloadProgress);
    connect(reply_, &QNetworkReply::finished, this, [this, manifest]() {
        OnFinished(manifest);
    });
}

void AsrModelArchiveDownloader::Cancel() {
    if (reply_ == nullptr) {
        return;
    }
    cancel_requested_ = true;
    reply_->abort();
}

void AsrModelArchiveDownloader::OnReadyRead() {
    if (reply_ == nullptr) {
        return;
    }

    const QByteArray bytes = reply_->readAll();
    if (bytes.isEmpty()) {
        return;
    }

    const qint64 written = output_file_.write(bytes);
    if (written != bytes.size()) {
        FinishWithFailure(QStringLiteral("Failed to write ASR model archive cache: %1").arg(output_file_.errorString()));
        return;
    }

    hash_.addData(bytes);
    bytes_received_ += bytes.size();
}

void AsrModelArchiveDownloader::OnDownloadProgress(qint64 bytes_received, qint64 bytes_total) {
    bytes_total_ = bytes_total;
    emit ProgressChanged(bytes_received, bytes_total);
}

void AsrModelArchiveDownloader::OnFinished(const AsrModelManifest &manifest) {
    if (reply_ == nullptr) {
        return;
    }

    OnReadyRead();
    if (reply_ == nullptr) {
        return;
    }

    if (cancel_requested_) {
        FinishWithFailure(QStringLiteral("ASR model archive download cancelled"));
        return;
    }

    if (reply_->error() != QNetworkReply::NoError) {
        FinishWithFailure(QStringLiteral("ASR model archive download failed: %1").arg(reply_->errorString()));
        return;
    }

    if (manifest.archive_size_bytes > 0 && bytes_received_ != manifest.archive_size_bytes) {
        FinishWithFailure(QStringLiteral("ASR model archive size mismatch: expected %1 bytes, got %2 bytes")
                              .arg(manifest.archive_size_bytes)
                              .arg(bytes_received_));
        return;
    }

    if (!output_file_.flush()) {
        FinishWithFailure(QStringLiteral("Failed to flush ASR model archive cache: %1").arg(output_file_.errorString()));
        return;
    }
    output_file_.close();

    const QString actual_sha256 = QString::fromLatin1(hash_.result().toHex());
    if (actual_sha256.compare(manifest.archive_sha256.trimmed(), Qt::CaseInsensitive) != 0) {
        FinishWithFailure(QStringLiteral("ASR model archive checksum mismatch: expected %1 got %2")
                              .arg(manifest.archive_sha256.trimmed(), actual_sha256));
        return;
    }

    const QString backup_path = archive_path_ + QStringLiteral(".previous");
    QFile::remove(backup_path);
    const bool had_existing_archive = QFileInfo(archive_path_).exists();
    if (had_existing_archive && !QFile::rename(archive_path_, backup_path)) {
        FinishWithFailure(QStringLiteral("Failed to prepare existing ASR model archive cache file for replacement"));
        return;
    }

    if (!QFile::rename(part_path_, archive_path_)) {
        if (had_existing_archive) {
            QFile::rename(backup_path, archive_path_);
        }
        FinishWithFailure(QStringLiteral("Failed to activate ASR model archive cache file"));
        return;
    }
    if (had_existing_archive) {
        QFile::remove(backup_path);
    }

    FinishWithSuccess();
}

void AsrModelArchiveDownloader::FinishWithSuccess() {
    if (output_file_.isOpen()) {
        output_file_.close();
    }

    AsrModelArchiveDownloadResult result;
    result.success = true;
    result.archive_path = archive_path_;
    result.bytes_received = bytes_received_;
    result.bytes_total = bytes_total_;
    CleanupReply();
    emit Finished(result);
}

void AsrModelArchiveDownloader::FinishWithFailure(const QString &message) {
    AsrModelArchiveDownloadResult result;
    result.success = false;
    result.archive_path = archive_path_;
    result.error_message = message;
    result.bytes_received = bytes_received_;
    result.bytes_total = bytes_total_;
    CleanupPartFile();
    CleanupReply();
    emit Finished(result);
}

void AsrModelArchiveDownloader::CleanupReply() {
    if (reply_ == nullptr) {
        return;
    }
    reply_->disconnect(this);
    reply_->deleteLater();
    reply_ = nullptr;
}

void AsrModelArchiveDownloader::CleanupPartFile() {
    if (output_file_.isOpen()) {
        output_file_.close();
    }
    if (!part_path_.isEmpty()) {
        QFile::remove(part_path_);
    }
}

}  // namespace shatv::app
