#include <QtTest>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "app/asr_model_service.h"

namespace {

using shatv::app::AsrModelInstallSource;
using shatv::app::AsrModelInstallStatus;
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

void WriteRequiredModelFiles(const QString &model_dir) {
    QDir dir(model_dir);
    QVERIFY(dir.mkpath(QStringLiteral(".")));
    WriteEmptyFile(dir.filePath(QStringLiteral("encoder.int8.onnx")));
    WriteEmptyFile(dir.filePath(QStringLiteral("decoder.int8.onnx")));
    WriteEmptyFile(dir.filePath(QStringLiteral("tokens.txt")));
}

class AsrModelServiceTest final : public QObject {
    Q_OBJECT

   private slots:
    void parses_manifest_json();
    void rejects_manifest_json_with_missing_required_field();
    void reports_not_installed_for_missing_app_managed_directory();
    void reports_incomplete_with_missing_required_files();
    void reports_installed_for_complete_app_managed_directory();
    void applies_file_name_environment_overrides();
    void reports_developer_override_for_complete_environment_directory();
    void invalid_environment_directory_does_not_fall_back_to_app_managed_model();
};

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
    WriteEmptyFile(install_dir.filePath(QStringLiteral("encoder.int8.onnx")));

    const auto status = service.InstalledModelStatus();

    QCOMPARE(EnumValue(status.status), EnumValue(AsrModelInstallStatus::kIncomplete));
    QCOMPARE(status.missing_files.size(), 2);
    QVERIFY(status.missing_files.at(0).endsWith(QStringLiteral("decoder.int8.onnx")));
    QVERIFY(status.missing_files.at(1).endsWith(QStringLiteral("tokens.txt")));
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

}  // namespace

QTEST_GUILESS_MAIN(AsrModelServiceTest)

#include "asr_model_service_test.moc"
