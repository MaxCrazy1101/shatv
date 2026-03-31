#include <iostream>

#include <QApplication>
#include <QMainWindow>
#include <QTimer>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("ShaTV");
    window.resize(1280, 720);

    const bool smoke_test = app.arguments().contains("--smoke-test");
    if (smoke_test) {
        window.show();
        std::cout << "ShaTV Qt6 bootstrap" << std::endl;
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
        return app.exec();
    }

    window.show();
    return app.exec();
}
