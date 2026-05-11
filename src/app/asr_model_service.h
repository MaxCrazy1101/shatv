#pragma once

#include <optional>

#include <QString>
#include <QStringList>

namespace shatv::app {

struct AsrModelFileSet {
    QString encoder_name = QStringLiteral("encoder.int8.onnx");
    QString decoder_name = QStringLiteral("decoder.int8.onnx");
    QString tokens_name = QStringLiteral("tokens.txt");
};

struct AsrModelManifest {
    QString id;
    QString version;
    QString display_name;
    QString source_url;
    qint64 archive_size_bytes = 0;
    qint64 installed_size_bytes = 0;
    QString archive_sha256;
    QString license;
    QString attribution;
    AsrModelFileSet files;
};

enum class AsrModelInstallStatus {
    kNotInstalled,
    kIncomplete,
    kInstalled,
    kDeveloperOverride,
};

enum class AsrModelInstallSource {
    kAppManaged,
    kDeveloperOverride,
};

struct AsrModelStatus {
    AsrModelInstallStatus status = AsrModelInstallStatus::kNotInstalled;
    AsrModelInstallSource source = AsrModelInstallSource::kAppManaged;
    QString model_dir;
    QStringList missing_files;
    QString message;

    bool Available() const;
};

class AsrModelService final {
   public:
    explicit AsrModelService(QString model_root = DefaultModelRoot(),
                             AsrModelManifest manifest = DefaultManifest());

    static QString DefaultModelRoot();
    static AsrModelManifest DefaultManifest();
    static std::optional<AsrModelManifest> ManifestFromJson(const QByteArray &json,
                                                            QString *error_message = nullptr);

    const AsrModelManifest &SelectedManifest() const;
    AsrModelFileSet EffectiveFiles() const;
    QString InstallDirectory() const;
    AsrModelStatus InstalledModelStatus() const;

   private:
    AsrModelStatus StatusForDirectory(const QString &model_dir,
                                      AsrModelInstallSource source,
                                      bool directory_required) const;

    QString model_root_;
    AsrModelManifest manifest_;
};

}  // namespace shatv::app
