#pragma once

#include <QString>

#include "app/asr_model_service.h"

namespace shatv::app {

struct AsrModelArchiveInstallResult {
    bool success = false;
    QString install_dir;
    QString error_message;
};

class AsrModelArchiveInstaller final {
   public:
    explicit AsrModelArchiveInstaller(QString model_root = AsrModelService::DefaultModelRoot());

    static bool Supported();

    AsrModelArchiveInstallResult InstallVerifiedArchive(const QString &archive_path,
                                                        const AsrModelManifest &manifest) const;

   private:
    QString model_root_;
};

}  // namespace shatv::app
