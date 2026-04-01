#include <QApplication>

#include "app/application.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    const bool smoke_test = app.arguments().contains("--smoke-test");

    shatv::app::Application application(&app, smoke_test);
    return application.Run();
}
