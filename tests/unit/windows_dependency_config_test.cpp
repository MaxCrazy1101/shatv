#include <QFile>
#include <QtTest>

namespace {

class WindowsDependencyConfigTest : public QObject {
    Q_OBJECT

   private slots:
    void linux_style_mpv_target_name_is_not_embedded_in_src_cmakelists();
    void windows_portable_variables_are_declared_in_top_level_cmake();
};

void WindowsDependencyConfigTest::linux_style_mpv_target_name_is_not_embedded_in_src_cmakelists() {
    QFile file(QStringLiteral(SHATV_SRC_CMAKELISTS));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(file.readAll());

    QVERIFY(!text.contains(QStringLiteral("PkgConfig::MPV")));
    QVERIFY(text.contains(QStringLiteral("shatv_mpv")));
}

void WindowsDependencyConfigTest::windows_portable_variables_are_declared_in_top_level_cmake() {
    QFile file(QStringLiteral(SHATV_TOPLEVEL_CMAKELISTS));
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString text = QString::fromUtf8(file.readAll());

    QVERIFY(text.contains(QStringLiteral("SHATV_MPV_INCLUDE_DIR")));
    QVERIFY(text.contains(QStringLiteral("SHATV_MPV_LIBRARY")));
    QVERIFY(text.contains(QStringLiteral("SHATV_MPV_DLL")));
}

}  // namespace

QTEST_GUILESS_MAIN(WindowsDependencyConfigTest)

#include "windows_dependency_config_test.moc"
