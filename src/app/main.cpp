#include <clocale>

#include <QApplication>

#include "app/application.h"
#include "app/launch_options.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // Qt 初始化后会覆盖 LC_NUMERIC，这里统一钉回 C 供 libmpv 使用。
    std::setlocale(LC_NUMERIC, "C");

    const shatv::app::LaunchOptions options = shatv::app::ParseLaunchOptions(app.arguments());
    shatv::app::Application application(&app, options);
    return application.Run();
}
