#include <QtTest>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include "app/asr_model_service.h"

namespace {

using shatv::app::AsrModelInstallSource;
using shatv::app::AsrModelInstallStatus;
using shatv::app::AsrModelArchiveDownloadResult;
using shatv::app::AsrModelArchiveDownloader;
using shatv::app::AsrModelManifest;
using shatv::app::AsrModelService;

class ScopedEnvironmentVariable final {
   public:
    explicit ScopedEnvironmentVariable(const char *name) : name_(name), had_value_(qEnvironmentVariableIsSet(name)) {
        if (had_value_) {
            old_value_ = qgetenv(name);
        }
        qunsetenv(name_);
    }

    ~ScopedEnvironmentVariable() {
        if (had_value_) {
            qputenv(name_, old_value_);
        } else {
            qunsetenv(name_);
        }
    }

    ScopedEnvironmentVariable(const ScopedEnvironmentVariable &) = delete;
    ScopedEnvironmentVariable &operator=(const ScopedEnvironmentVariable &) = delete;

   private:
    const char *name_ = nullptr;
    bool had_value_ = false;
    QByteArray old_value_;
};

int EnumValue(AsrModelInstallStatus status) {
    return static_cast<int>(status);
}

int EnumValue(AsrModelInstallSource source) {
    return static_cast<int>(source);
}

void WriteEmptyFile(const QString &path) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
}

void WriteFile(const QString &path, const QByteArray &content) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(content), static_cast<qint64>(content.size()));
    file.close();
}

QByteArray ReadFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

QString Sha256Hex(const QByteArray &content) {
    return QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
}

void WriteRequiredModelFiles(const QString &model_dir) {
    QDir dir(model_dir);
    QVERIFY(dir.mkpath(QStringLiteral(".")));
    const AsrModelManifest manifest = AsrModelService::DefaultManifest();
    WriteEmptyFile(dir.filePath(manifest.files.encoder_name));
    WriteEmptyFile(dir.filePath(manifest.files.decoder_name));
    WriteEmptyFile(dir.filePath(manifest.files.tokens_name));
}

class HangingNetworkReply final : public QNetworkReply {
   public:
    HangingNetworkReply(const QNetworkRequest &request, QByteArray payload, QObject *parent = nullptr)
        : QNetworkReply(parent), payload_(std::move(payload)) {
        setRequest(request);
        setUrl(request.url());
        setHeader(QNetworkRequest::ContentLengthHeader, payload_.size() * 2);
        open(QIODevice::ReadOnly | QIODevice::Unbuffered);

        QTimer::singleShot(0, this, [this]() {
            if (aborted_) {
                return;
            }
            emit readyRead();
            emit downloadProgress(payload_.size(), payload_.size() * 2);
        });
    }

    void abort() override {
        aborted_ = true;
        setError(QNetworkReply::OperationCanceledError, QStringLiteral("operation canceled"));
    }

    qint64 bytesAvailable() const override {
        return payload_.size() - offset_ + QNetworkReply::bytesAvailable();
    }

   protected:
    qint64 readData(char *data, qint64 max_size) override {
        const qint64 remaining = payload_.size() - offset_;
        const qint64 bytes_to_read = qMin(max_size, remaining);
        if (bytes_to_read <= 0) {
            return -1;
        }

        memcpy(data, payload_.constData() + offset_, static_cast<size_t>(bytes_to_read));
        offset_ += bytes_to_read;
        return bytes_to_read;
    }

    qint64 writeData(const char *, qint64) override {
        return -1;
    }

   private:
    QByteArray payload_;
    qint64 offset_ = 0;
    bool aborted_ = false;
};

class HangingNetworkAccessManager final : public QNetworkAccessManager {
   public:
    explicit HangingNetworkAccessManager(QByteArray payload, QObject *parent = nullptr)
        : QNetworkAccessManager(parent), payload_(std::move(payload)) {}

   protected:
    QNetworkReply *createRequest(Operation operation,
                                 const QNetworkRequest &request,
                                 QIODevice *outgoing_data = nullptr) override {
        Q_UNUSED(operation);
        Q_UNUSED(outgoing_data);
        return new HangingNetworkReply(request, payload_, this);
    }

   private:
    QByteArray payload_;
};

class AsrModelServiceTest final : public QObject {
    Q_OBJECT

   private slots:
    void initTestCase();
    void default_manifest_matches_downloaded_archive_file_names();
    void parses_manifest_json();
    void rejects_manifest_json_with_unsafe_id();
    void rejects_manifest_json_with_missing_required_field();
    void reports_not_installed_for_missing_app_managed_directory();
    void reports_incomplete_with_missing_required_files();
    void reports_installed_for_complete_app_managed_directory();
    void applies_file_name_environment_overrides();
    void reports_developer_override_for_complete_environment_directory();
    void invalid_environment_directory_does_not_fall_back_to_app_managed_model();
    void default_storage_roots_use_separate_model_and_archive_directories();
    void empty_default_model_root_reports_unavailable_directory();
    void unsafe_manifest_id_disables_app_managed_install_directory();
    void downloads_archive_to_cache_and_verifies_sha256();
    void reuses_existing_archive_cache_after_sha256_verification();
    void checksum_mismatch_removes_part_and_preserves_existing_archive();
    void replacing_invalid_existing_archive_preserves_content_when_activation_fails();
    void destroying_active_downloader_removes_part_and_preserves_existing_archive();
};

void AsrModelServiceTest::initTestCase() {
    qRegisterMetaType<AsrModelArchiveDownloadResult>();
}

void AsrModelServiceTest::default_manifest_matches_downloaded_archive_file_names() {
    const AsrModelManifest manifest = AsrModelService::DefaultManifest();

    QCOMPARE(manifest.id, QStringLiteral("sherpa-onnx-streaming-paraformer-bilingual-zh-en-int8"));
    QCOMPARE(manifest.display_name, QStringLiteral("Streaming Paraformer bilingual zh/en int8"));
    QCOMPARE(manifest.files.encoder_name, QStringLiteral("encoder.int8.onnx"));
    QCOMPARE(manifest.files.decoder_name, QStringLiteral("decoder.int8.onnx"));
    QCOMPARE(manifest.files.tokens_name, QStringLiteral("tokens.txt"));
}

void AsrModelServiceTest::parses_manifest_json() {
    QString error_message;
    const auto manifest = AsrModelService::ManifestFromJson(R"json(
        {
          "id": "model-id",
          "version": "v1",
          "display_name": "Display",
          "source_url": "https://example.invalid/model.zip",
          "archive_size_bytes": 123,
          "installed_size_bytes": 456,
          "archive_sha256": "abc",
          "license": "license",
          "attribution": "source",
          "files": {
            "encoder": "encoder.onnx",
            "decoder": "decoder.onnx",
            "tokens": "tokens.txt"
          }
        }
    )json",
                                                            &error_message);

    QVERIFY2(manifest.has_value(), qPrintable(error_message));
    QCOMPARE(manifest->id, QStringLiteral("model-id"));
    QCOMPARE(manifest->version, QStringLiteral("v1"));
    QCOMPARE(manifest->display_name, QStringLiteral("Display"));
    QCOMPARE(manifest->source_url, QStringLiteral("https://example.invalid/model.zip"));
    QCOMPARE(manifest->archive_size_bytes, 123);
    QCOMPARE(manifest->installed_size_bytes, 456);
    QCOMPARE(manifest->archive_sha256, QStringLiteral("abc"));
    QCOMPARE(manifest->license, QStringLiteral("license"));
    QCOMPARE(manifest->attribution, QStringLiteral("source"));
    QCOMPARE(manifest->files.encoder_name, QStringLiteral("encoder.onnx"));
    QCOMPARE(manifest->files.decoder_name, QStringLiteral("decoder.onnx"));
    QCOMPARE(manifest->files.tokens_name, QStringLiteral("tokens.txt"));
}

void AsrModelServiceTest::rejects_manifest_json_with_unsafe_id() {
    QString error_message;
    const auto manifest = AsrModelService::ManifestFromJson(R"json(
        {
          "id": "../outside",
          "version": "v1",
          "display_name": "Display",
          "source_url": "https://example.invalid/model.zip",
          "archive_size_bytes": 123,
          "installed_size_bytes": 456,
          "archive_sha256": "abc",
          "license": "license",
          "attribution": "source",
          "files": {
            "encoder": "encoder.onnx",
            "decoder": "decoder.onnx",
            "tokens": "tokens.txt"
          }
        }
    )json",
                                                            &error_message);

    QVERIFY(!manifest.has_value());
    QCOMPARE(error_message, QStringLiteral("manifest field id contains unsafe path characters"));
}

void AsrModelServiceTest::rejects_manifest_json_with_missing_required_field() {
    QString error_message;
    const auto manifest = AsrModelService::ManifestFromJson(R"json(
        {
          "id": "model-id",
          "version": "v1"
        }
    )json",
                                                            &error_message);

    QVERIFY(!manifest.has_value());
    QCOMPARE(error_message, QStringLiteral("manifest field display_name must be a non-empty string"));
}

void AsrModelServiceTest::reports_not_installed_for_missing_app_managed_directory() {
    ScopedEnvironmentVariable model_dir_env("SHATV_ASR_MODEL_DIR");
    ScopedEnvironmentVariable encoder_env("SHATV_ASR_ENCODER_NAME");
    ScopedEnvironmentVariable decoder_env("SHATV_ASR_DECODER_NAME");
    ScopedEnvironmentVariable tokens_env("SHATV_ASR_TOKENS_NAME");
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const AsrModelService service(temp_dir.filePath(QStringLiteral("models")));
    const auto status = service.InstalledModelStatus();

    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kNotInstalled));
    QCOMPARE(EnumValue(status.source), EnumValue(AsrModelInstallSource::kAppManaged));
    QVERIFY(!status.Available());
}

void AsrModelServiceTest::reports_incomplete_with_missing_required_files() {
    ScopedEnvironmentVariable model_dir_env("SHATV_ASR_MODEL_DIR");
    ScopedEnvironmentVariable encoder_env("SHATV_ASR_ENCODER_NAME");
    ScopedEnvironmentVariable decoder_env("SHATV_ASR_DECODER_NAME");
    ScopedEnvironmentVariable tokens_env("SHATV_ASR_TOKENS_NAME");
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    AsrModelService service(temp_dir.filePath(QStringLiteral("models")));
    QDir install_dir(service.InstallDirectory());
    QVERIFY(install_dir.mkpath(QStringLiteral(".")));
    WriteEmptyFile(install_dir.filePath(service.SelectedManifest().files.encoder_name));

    const auto status = service.InstalledModelStatus();

    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kIncomplete));
    QCOMPARE(status.missing_files.size(), 2);
    QVERIFY(status.missing_files.at(0).endsWith(service.SelectedManifest().files.decoder_name));
    QVERIFY(status.missing_files.at(1).endsWith(service.SelectedManifest().files.tokens_name));
    QVERIFY(!status.Available());
}

void AsrModelServiceTest::reports_installed_for_complete_app_managed_directory() {
    ScopedEnvironmentVariable model_dir_env("SHATV_ASR_MODEL_DIR");
    ScopedEnvironmentVariable encoder_env("SHATV_ASR_ENCODER_NAME");
    ScopedEnvironmentVariable decoder_env("SHATV_ASR_DECODER_NAME");
    ScopedEnvironmentVariable tokens_env("SHATV_ASR_TOKENS_NAME");
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    AsrModelService service(temp_dir.filePath(QStringLiteral("models")));
    WriteRequiredModelFiles(service.InstallDirectory());

    const auto status = service.InstalledModelStatus();

    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kInstalled));
    QCOMPARE(EnumValue(status.source), EnumValue(AsrModelInstallSource::kAppManaged));
    QVERIFY(status.Available());
    QCOMPARE(status.model_dir, service.InstallDirectory());
}

void AsrModelServiceTest::applies_file_name_environment_overrides() {
    ScopedEnvironmentVariable model_dir_env("SHATV_ASR_MODEL_DIR");
    ScopedEnvironmentVariable encoder_env("SHATV_ASR_ENCODER_NAME");
    ScopedEnvironmentVariable decoder_env("SHATV_ASR_DECODER_NAME");
    ScopedEnvironmentVariable tokens_env("SHATV_ASR_TOKENS_NAME");
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    qputenv("SHATV_ASR_ENCODER_NAME", "encoder.custom.onnx");
    qputenv("SHATV_ASR_DECODER_NAME", "decoder.custom.onnx");
    qputenv("SHATV_ASR_TOKENS_NAME", "tokens.custom.txt");

    AsrModelService service(temp_dir.filePath(QStringLiteral("models")));
    QDir install_dir(service.InstallDirectory());
    QVERIFY(install_dir.mkpath(QStringLiteral(".")));
    WriteEmptyFile(install_dir.filePath(QStringLiteral("encoder.custom.onnx")));
    WriteEmptyFile(install_dir.filePath(QStringLiteral("decoder.custom.onnx")));
    WriteEmptyFile(install_dir.filePath(QStringLiteral("tokens.custom.txt")));

    const auto status = service.InstalledModelStatus();

    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kInstalled));
    QVERIFY(status.Available());
}

void AsrModelServiceTest::reports_developer_override_for_complete_environment_directory() {
    ScopedEnvironmentVariable model_dir_env("SHATV_ASR_MODEL_DIR");
    ScopedEnvironmentVariable encoder_env("SHATV_ASR_ENCODER_NAME");
    ScopedEnvironmentVariable decoder_env("SHATV_ASR_DECODER_NAME");
    ScopedEnvironmentVariable tokens_env("SHATV_ASR_TOKENS_NAME");
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    const QString developer_model_dir = temp_dir.filePath(QStringLiteral("developer-model"));
    WriteRequiredModelFiles(developer_model_dir);
    qputenv("SHATV_ASR_MODEL_DIR", developer_model_dir.toUtf8());

    const AsrModelService service(temp_dir.filePath(QStringLiteral("models")));
    const auto status = service.InstalledModelStatus();

    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kDeveloperOverride));
    QCOMPARE(EnumValue(status.source), EnumValue(AsrModelInstallSource::kDeveloperOverride));
    QVERIFY(status.Available());
    QCOMPARE(status.model_dir, developer_model_dir);
}

void AsrModelServiceTest::invalid_environment_directory_does_not_fall_back_to_app_managed_model() {
    ScopedEnvironmentVariable model_dir_env("SHATV_ASR_MODEL_DIR");
    ScopedEnvironmentVariable encoder_env("SHATV_ASR_ENCODER_NAME");
    ScopedEnvironmentVariable decoder_env("SHATV_ASR_DECODER_NAME");
    ScopedEnvironmentVariable tokens_env("SHATV_ASR_TOKENS_NAME");
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    AsrModelService service(temp_dir.filePath(QStringLiteral("models")));
    WriteRequiredModelFiles(service.InstallDirectory());
    const QString invalid_override = temp_dir.filePath(QStringLiteral("missing-developer-model"));
    qputenv("SHATV_ASR_MODEL_DIR", invalid_override.toUtf8());

    const auto status = service.InstalledModelStatus();

    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kIncomplete));
    QCOMPARE(EnumValue(status.source), EnumValue(AsrModelInstallSource::kDeveloperOverride));
    QVERIFY(!status.Available());
    QCOMPARE(status.model_dir, invalid_override);
}

void AsrModelServiceTest::default_storage_roots_use_separate_model_and_archive_directories() {
    const QString model_root = AsrModelService::DefaultModelRoot();
    const QString archive_cache_root = AsrModelService::DefaultArchiveCacheRoot();

    QVERIFY(!model_root.isEmpty());
    QVERIFY(!archive_cache_root.isEmpty());
    QVERIFY(model_root.endsWith(QStringLiteral("asr-models")));
    QVERIFY(archive_cache_root.endsWith(QStringLiteral("asr-model-archives")));
    QVERIFY(model_root != archive_cache_root);
}

void AsrModelServiceTest::empty_default_model_root_reports_unavailable_directory() {
    ScopedEnvironmentVariable model_dir_env("SHATV_ASR_MODEL_DIR");
    ScopedEnvironmentVariable encoder_env("SHATV_ASR_ENCODER_NAME");
    ScopedEnvironmentVariable decoder_env("SHATV_ASR_DECODER_NAME");
    ScopedEnvironmentVariable tokens_env("SHATV_ASR_TOKENS_NAME");

    const AsrModelService service(QString{});
    const auto status = service.InstalledModelStatus();

    QCOMPARE(service.InstallDirectory(), QString{});
    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kIncomplete));
    QCOMPARE(EnumValue(status.source), EnumValue(AsrModelInstallSource::kAppManaged));
    QCOMPARE(status.message, QStringLiteral("ASR model directory is unavailable"));
    QVERIFY(!status.Available());
}

void AsrModelServiceTest::unsafe_manifest_id_disables_app_managed_install_directory() {
    ScopedEnvironmentVariable model_dir_env("SHATV_ASR_MODEL_DIR");
    ScopedEnvironmentVariable encoder_env("SHATV_ASR_ENCODER_NAME");
    ScopedEnvironmentVariable decoder_env("SHATV_ASR_DECODER_NAME");
    ScopedEnvironmentVariable tokens_env("SHATV_ASR_TOKENS_NAME");
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());

    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("../outside");

    const AsrModelService service(temp_dir.filePath(QStringLiteral("models")), manifest);
    const auto status = service.InstalledModelStatus();

    QCOMPARE(service.InstallDirectory(), QString{});
    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kIncomplete));
    QCOMPARE(status.message, QStringLiteral("ASR model directory is unavailable"));
}

void AsrModelServiceTest::downloads_archive_to_cache_and_verifies_sha256() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QByteArray archive_bytes("test model archive payload");
    const QString source_path = temp_dir.filePath(QStringLiteral("model.tar.bz2"));
    WriteFile(source_path, archive_bytes);

    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("test-model");
    manifest.source_url = QUrl::fromLocalFile(source_path).toString();
    manifest.archive_size_bytes = archive_bytes.size();
    manifest.archive_sha256 = Sha256Hex(archive_bytes);

    QNetworkAccessManager network_manager;
    AsrModelArchiveDownloader downloader(&network_manager, temp_dir.filePath(QStringLiteral("cache")));
    QSignalSpy progress_spy(&downloader, &AsrModelArchiveDownloader::ProgressChanged);
    QSignalSpy finished_spy(&downloader, &AsrModelArchiveDownloader::Finished);

    downloader.Start(manifest);

    QVERIFY(!finished_spy.isEmpty() || finished_spy.wait(5000));
    QCOMPARE(finished_spy.size(), 1);
    const auto result = qvariant_cast<AsrModelArchiveDownloadResult>(finished_spy.takeFirst().at(0));

    QVERIFY2(result.success, qPrintable(result.error_message));
    QVERIFY(QFileInfo(result.archive_path).isFile());
    QVERIFY(result.archive_path.endsWith(QStringLiteral(".tar.bz2")));
    QCOMPARE(ReadFile(result.archive_path), archive_bytes);
    QVERIFY(!QFileInfo(result.archive_path + QStringLiteral(".part")).exists());
    QVERIFY(!progress_spy.isEmpty());
}

void AsrModelServiceTest::reuses_existing_archive_cache_after_sha256_verification() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QByteArray archive_bytes("cached verified archive");

    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("test-model");
    manifest.source_url = QStringLiteral("https://example.invalid/should-not-download.tar.bz2");
    manifest.archive_size_bytes = archive_bytes.size();
    manifest.archive_sha256 = Sha256Hex(archive_bytes);

    QNetworkAccessManager network_manager;
    AsrModelArchiveDownloader downloader(&network_manager, temp_dir.filePath(QStringLiteral("cache")));
    const QString archive_path = downloader.ArchivePath(manifest);
    QVERIFY(QDir().mkpath(QFileInfo(archive_path).path()));
    WriteFile(archive_path, archive_bytes);
    QSignalSpy progress_spy(&downloader, &AsrModelArchiveDownloader::ProgressChanged);
    QSignalSpy finished_spy(&downloader, &AsrModelArchiveDownloader::Finished);

    downloader.Start(manifest);

    QVERIFY(!finished_spy.isEmpty() || finished_spy.wait(5000));
    QCOMPARE(finished_spy.size(), 1);
    const auto result = qvariant_cast<AsrModelArchiveDownloadResult>(finished_spy.takeFirst().at(0));

    QVERIFY2(result.success, qPrintable(result.error_message));
    QCOMPARE(result.archive_path, archive_path);
    QCOMPARE(ReadFile(archive_path), archive_bytes);
    QVERIFY(progress_spy.isEmpty());
}

void AsrModelServiceTest::checksum_mismatch_removes_part_and_preserves_existing_archive() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QByteArray previous_archive("previous verified archive");
    const QByteArray downloaded_archive("downloaded archive with wrong checksum");
    const QString source_path = temp_dir.filePath(QStringLiteral("model.zip"));
    WriteFile(source_path, downloaded_archive);

    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("test-model");
    manifest.source_url = QUrl::fromLocalFile(source_path).toString();
    manifest.archive_size_bytes = downloaded_archive.size();
    manifest.archive_sha256 = QString(64, QLatin1Char('0'));

    QNetworkAccessManager network_manager;
    AsrModelArchiveDownloader downloader(&network_manager, temp_dir.filePath(QStringLiteral("cache")));
    const QString archive_path = downloader.ArchivePath(manifest);
    QVERIFY(QDir().mkpath(QFileInfo(archive_path).path()));
    WriteFile(archive_path, previous_archive);
    QSignalSpy finished_spy(&downloader, &AsrModelArchiveDownloader::Finished);

    downloader.Start(manifest);

    QVERIFY(!finished_spy.isEmpty() || finished_spy.wait(5000));
    QCOMPARE(finished_spy.size(), 1);
    const auto result = qvariant_cast<AsrModelArchiveDownloadResult>(finished_spy.takeFirst().at(0));

    QVERIFY(!result.success);
    QVERIFY(result.error_message.contains(QStringLiteral("checksum mismatch")));
    QCOMPARE(ReadFile(archive_path), previous_archive);
    QVERIFY(!QFileInfo(archive_path + QStringLiteral(".part")).exists());
}

void AsrModelServiceTest::replacing_invalid_existing_archive_preserves_content_when_activation_fails() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QByteArray previous_archive("previous invalid archive");
    const QByteArray downloaded_archive("downloaded replacement archive");
    const QString source_path = temp_dir.filePath(QStringLiteral("model.tar.bz2"));
    WriteFile(source_path, downloaded_archive);

    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("test-model");
    manifest.source_url = QUrl::fromLocalFile(source_path).toString();
    manifest.archive_size_bytes = downloaded_archive.size();
    manifest.archive_sha256 = Sha256Hex(downloaded_archive);

    QNetworkAccessManager network_manager;
    AsrModelArchiveDownloader downloader(&network_manager, temp_dir.filePath(QStringLiteral("cache")));
    const QString archive_path = downloader.ArchivePath(manifest);
    QVERIFY(QDir().mkpath(QFileInfo(archive_path).path()));
    QVERIFY(QDir().mkpath(archive_path));
    WriteFile(QDir(archive_path).filePath(QStringLiteral("keep.txt")), previous_archive);
    QSignalSpy finished_spy(&downloader, &AsrModelArchiveDownloader::Finished);

    downloader.Start(manifest);

    QVERIFY(!finished_spy.isEmpty() || finished_spy.wait(5000));
    QCOMPARE(finished_spy.size(), 1);
    const auto result = qvariant_cast<AsrModelArchiveDownloadResult>(finished_spy.takeFirst().at(0));

    QVERIFY(!result.success);
    QVERIFY(result.error_message.contains(QStringLiteral("archive cache path exists and is not a file")));
    QVERIFY(QFileInfo(archive_path).isDir());
    QCOMPARE(ReadFile(QDir(archive_path).filePath(QStringLiteral("keep.txt"))), previous_archive);
    QVERIFY(!QFileInfo(archive_path + QStringLiteral(".part")).exists());
}

void AsrModelServiceTest::destroying_active_downloader_removes_part_and_preserves_existing_archive() {
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    const QByteArray previous_archive("previous verified archive");
    const QByteArray partial_payload("partial replacement bytes");

    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("test-model");
    manifest.source_url = QStringLiteral("https://example.invalid/model.tar.bz2");
    manifest.archive_size_bytes = partial_payload.size() * 2;
    manifest.archive_sha256 = Sha256Hex(QByteArray("complete archive bytes"));

    HangingNetworkAccessManager network_manager(partial_payload);
    const QString cache_root = temp_dir.filePath(QStringLiteral("cache"));
    auto downloader = std::make_unique<AsrModelArchiveDownloader>(&network_manager, cache_root);
    const QString archive_path = downloader->ArchivePath(manifest);
    QVERIFY(QDir().mkpath(QFileInfo(archive_path).path()));
    WriteFile(archive_path, previous_archive);

    downloader->Start(manifest);

    QTRY_VERIFY(QFileInfo(archive_path + QStringLiteral(".part")).isFile());
    downloader->Cancel();
    downloader.reset();

    QCOMPARE(ReadFile(archive_path), previous_archive);
    QVERIFY(!QFileInfo(archive_path + QStringLiteral(".part")).exists());
}

}  // namespace

QTEST_GUILESS_MAIN(AsrModelServiceTest)

#include "asr_model_service_test.moc"
