#include <QApplication>

#include "app/application.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    const bool smoke_test = app.arguments().contains("--smoke-test");
    const bool mpv_smoke = app.arguments().contains("--mpv-smoke");

    shatv::app::Application application(&app, smoke_test, mpv_smoke);
    return application.Run();
}
