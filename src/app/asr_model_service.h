#pragma once

#include <optional>

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;
class QNetworkReply;

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

struct AsrModelArchiveDownloadResult {
    bool success = false;
    QString archive_path;
    QString error_message;
    qint64 bytes_received = 0;
    qint64 bytes_total = -1;
};

bool IsSafeAsrModelId(const QString &id);

class AsrModelService final {
   public:
    explicit AsrModelService(QString model_root = DefaultModelRoot(),
                             AsrModelManifest manifest = DefaultManifest());

    static QString DefaultModelRoot();
    static QString DefaultArchiveCacheRoot();
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

class AsrModelArchiveDownloader final : public QObject {
    Q_OBJECT

   public:
    explicit AsrModelArchiveDownloader(QNetworkAccessManager *network_manager,
                                       QString archive_cache_root = AsrModelService::DefaultArchiveCacheRoot(),
                                       QObject *parent = nullptr);
    ~AsrModelArchiveDownloader() override;

    QString ArchivePath(const AsrModelManifest &manifest) const;
    void Start(const AsrModelManifest &manifest);
    void Cancel();

   signals:
    void ProgressChanged(qint64 bytes_received, qint64 bytes_total);
    void Finished(const shatv::app::AsrModelArchiveDownloadResult &result);

   private:
    void OnReadyRead();
    void OnDownloadProgress(qint64 bytes_received, qint64 bytes_total);
    void OnFinished(const AsrModelManifest &manifest);
    void FinishWithSuccess();
    void FinishWithFailure(const QString &message);
    void CleanupReply();
    void CleanupPartFile();

    QNetworkAccessManager *network_manager_ = nullptr;
    QString archive_cache_root_;
    QNetworkReply *reply_ = nullptr;
    QFile output_file_;
    QCryptographicHash hash_{QCryptographicHash::Sha256};
    QString archive_path_;
    QString part_path_;
    qint64 bytes_received_ = 0;
    qint64 bytes_total_ = -1;
    bool cancel_requested_ = false;
};

}  // namespace shatv::app

Q_DECLARE_METATYPE(shatv::app::AsrModelArchiveDownloadResult)
