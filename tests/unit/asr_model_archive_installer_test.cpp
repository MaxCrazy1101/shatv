#include <QtTest>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include "app/asr_model_archive_installer.h"

#if SHATV_HAS_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace {

using shatv::app::AsrModelArchiveInstaller;
using shatv::app::AsrModelManifest;
using shatv::app::AsrModelService;

#if SHATV_HAS_LIBARCHIVE
void WriteFile(const QString &path, const QByteArray &content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(content), static_cast<qint64>(content.size()));
}

QByteArray ReadFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

void WriteRequiredModelFiles(const QString &model_dir, const QByteArray &encoder_content) {
    const AsrModelManifest manifest = AsrModelService::DefaultManifest();
    WriteFile(QDir(model_dir).filePath(manifest.files.encoder_name), encoder_content);
    WriteFile(QDir(model_dir).filePath(manifest.files.decoder_name), QByteArray("decoder"));
    WriteFile(QDir(model_dir).filePath(QStringLiteral("tokens.txt")), QByteArray("tokens"));
}

void WriteArchiveEntry(archive *writer, const QString &path, const QByteArray &content) {
    archive_entry *entry = archive_entry_new();
    QVERIFY(entry != nullptr);
    const QByteArray path_bytes = path.toUtf8();
    archive_entry_set_pathname(entry, path_bytes.constData());
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0644);
    archive_entry_set_size(entry, content.size());
    QCOMPARE(archive_write_header(writer, entry), ARCHIVE_OK);
    QCOMPARE(archive_write_data(writer, content.constData(), content.size()), static_cast<la_ssize_t>(content.size()));
    archive_entry_free(entry);
}

void WriteTarBz2Archive(const QString &archive_path, const QList<QPair<QString, QByteArray>> &files) {
    archive *writer = archive_write_new();
    QVERIFY(writer != nullptr);
    QCOMPARE(archive_write_add_filter_bzip2(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_set_format_pax_restricted(writer), ARCHIVE_OK);
    const QByteArray archive_path_bytes = QFile::encodeName(archive_path);
    QCOMPARE(archive_write_open_filename(writer, archive_path_bytes.constData()), ARCHIVE_OK);
    for (const auto &file : files) {
        WriteArchiveEntry(writer, file.first, file.second);
    }
    QCOMPARE(archive_write_close(writer), ARCHIVE_OK);
    QCOMPARE(archive_write_free(writer), ARCHIVE_OK);
}
#endif

class AsrModelArchiveInstallerTest final : public QObject {
    Q_OBJECT

   private slots:
    void installs_tar_bz2_archive_after_required_file_validation();
    void rejects_path_traversal_and_preserves_existing_install();
    void rejects_archive_missing_required_files();
};

void AsrModelArchiveInstallerTest::installs_tar_bz2_archive_after_required_file_validation() {
    if (!AsrModelArchiveInstaller::Supported()) {
        QSKIP("libarchive support is not available in this build");
    }

#if SHATV_HAS_LIBARCHIVE
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("test-model");
    manifest.version = QStringLiteral("v-test");

    const QString archive_path = temp_dir.filePath(QStringLiteral("model.tar.bz2"));
    WriteTarBz2Archive(archive_path,
                       {
                           {QStringLiteral("model/") + manifest.files.encoder_name, QByteArray("new encoder")},
                           {QStringLiteral("model/") + manifest.files.decoder_name, QByteArray("new decoder")},
                           {QStringLiteral("model/") + manifest.files.tokens_name, QByteArray("new tokens")},
                       });

    const QString model_root = temp_dir.filePath(QStringLiteral("models"));
    const AsrModelArchiveInstaller installer(model_root);
    const auto result = installer.InstallVerifiedArchive(archive_path, manifest);

    QVERIFY2(result.success, qPrintable(result.error_message));
    QCOMPARE(result.install_dir, QDir(model_root).filePath(manifest.id));
    QCOMPARE(ReadFile(QDir(result.install_dir).filePath(manifest.files.encoder_name)), QByteArray("new encoder"));
    QVERIFY(QFileInfo(QDir(result.install_dir).filePath(QStringLiteral("asr_model_manifest.json"))).isFile());

    const QJsonDocument metadata =
        QJsonDocument::fromJson(ReadFile(QDir(result.install_dir).filePath(QStringLiteral("asr_model_manifest.json"))));
    QVERIFY(metadata.isObject());
    QCOMPARE(metadata.object().value(QStringLiteral("id")).toString(), manifest.id);
#endif
}

void AsrModelArchiveInstallerTest::rejects_path_traversal_and_preserves_existing_install() {
    if (!AsrModelArchiveInstaller::Supported()) {
        QSKIP("libarchive support is not available in this build");
    }

#if SHATV_HAS_LIBARCHIVE
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("test-model");

    const QString archive_path = temp_dir.filePath(QStringLiteral("malicious.tar.bz2"));
    WriteTarBz2Archive(archive_path,
                       {
                           {QStringLiteral("../evil.txt"), QByteArray("evil")},
                           {QStringLiteral("model/") + manifest.files.encoder_name, QByteArray("new encoder")},
                           {QStringLiteral("model/") + manifest.files.decoder_name, QByteArray("new decoder")},
                           {QStringLiteral("model/") + manifest.files.tokens_name, QByteArray("new tokens")},
                       });

    const QString model_root = temp_dir.filePath(QStringLiteral("models"));
    const QString existing_model_dir = QDir(model_root).filePath(manifest.id);
    WriteRequiredModelFiles(existing_model_dir, QByteArray("old encoder"));

    const AsrModelArchiveInstaller installer(model_root);
    const auto result = installer.InstallVerifiedArchive(archive_path, manifest);

    QVERIFY(!result.success);
    QVERIFY(result.error_message.contains(QStringLiteral("Unsafe archive entry path")));
    QCOMPARE(ReadFile(QDir(existing_model_dir).filePath(manifest.files.encoder_name)), QByteArray("old encoder"));
    QVERIFY(!QFileInfo(temp_dir.filePath(QStringLiteral("evil.txt"))).exists());
#endif
}

void AsrModelArchiveInstallerTest::rejects_archive_missing_required_files() {
    if (!AsrModelArchiveInstaller::Supported()) {
        QSKIP("libarchive support is not available in this build");
    }

#if SHATV_HAS_LIBARCHIVE
    QTemporaryDir temp_dir;
    QVERIFY(temp_dir.isValid());
    AsrModelManifest manifest = AsrModelService::DefaultManifest();
    manifest.id = QStringLiteral("test-model");

    const QString archive_path = temp_dir.filePath(QStringLiteral("incomplete.tar.bz2"));
    WriteTarBz2Archive(archive_path,
                       {
                           {QStringLiteral("model/") + manifest.files.encoder_name, QByteArray("encoder only")},
                       });

    const QString model_root = temp_dir.filePath(QStringLiteral("models"));

    const AsrModelArchiveInstaller installer(model_root);
    const auto result = installer.InstallVerifiedArchive(archive_path, manifest);

    QVERIFY(!result.success);
    QVERIFY(result.error_message.contains(QStringLiteral("missing required files")));
    QVERIFY(!QFileInfo(QDir(model_root).filePath(manifest.id)).exists());
#endif
}

}  // namespace

QTEST_GUILESS_MAIN(AsrModelArchiveInstallerTest)

#include "asr_model_archive_installer_test.moc"
