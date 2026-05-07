#include <clocale>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QQuickStyle>
#include <QTranslator>

#include "app/application.h"
#include "app/build_info.h"
#include "app/launch_options.h"
#include "app/logging.h"

int main(int argc, char *argv[]) {
#ifdef Q_OS_WIN
    // Windows 默认 Quick Controls style 不适合当前这种深色自定义外观，钉到 Fusion 保持和 Linux 一致。
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
#endif
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QString::fromUtf8(shatv::app::kProjectVersion));
    // Qt 初始化后会覆盖 LC_NUMERIC，这里统一钉回 C 供 FFmpeg 等媒体库使用。
    std::setlocale(LC_NUMERIC, "C");

    shatv::app::InitializeLogging();
    qCInfo(shatv::app::log_app).noquote()
        << "ShaTV startup"
        << "version=" << shatv::app::kProjectVersion
        << "build=" << shatv::app::kBuildId
        << "qt=" << qVersion()
        << "platform=" << QGuiApplication::platformName()
        << "logFile=" << shatv::app::CurrentLogFilePath();

    QTranslator translator;
    if (translator.load(QLocale(), "shatv", "_", ":/i18n")) {
        app.installTranslator(&translator);
    }

    const shatv::app::LaunchOptions options = shatv::app::ParseLaunchOptions(app.arguments());
    int exit_code = 0;
    {
        shatv::app::Application application(&app, options);
        exit_code = application.Run();
    }

    qCInfo(shatv::app::log_app) << "ShaTV shutdown" << "exitCode=" << exit_code;
    shatv::app::ShutdownLogging();
    return exit_code;
}
