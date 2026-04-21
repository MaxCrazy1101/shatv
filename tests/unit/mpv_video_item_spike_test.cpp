#include <QtTest>

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QUrl>

#include "player/mpv_player_backend.h"
#include "ui/qml_spike/mpv_video_item.h"

namespace {

void ConfigureSpikeGraphicsBackend() {
    qputenv("QSG_RENDER_LOOP", "basic");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
}

class DummySpikePlaybackBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString playbackState READ playbackState CONSTANT)
    Q_PROPERTY(QString sourceLabel READ sourceLabel CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage CONSTANT)
    Q_PROPERTY(bool videoReady READ videoReady CONSTANT)

   public:
    QString playbackState() const { return QStringLiteral("Idle"); }
    QString sourceLabel() const { return QStringLiteral("test-media"); }
    QString statusMessage() const { return QString(); }
    bool videoReady() const { return false; }

    Q_INVOKABLE void togglePlayPause() {}
    Q_INVOKABLE void stop() {}
};

class MpvVideoItemSpikeTest : public QObject {
    Q_OBJECT

   private slots:
    void qml_spike_window_loads_video_item();
    void qml_spike_window_marks_video_item_ready_after_backend_attach();
};

void MpvVideoItemSpikeTest::qml_spike_window_loads_video_item() {
    ConfigureSpikeGraphicsBackend();
    shatv::ui::qml_spike::RegisterQmlVideoTypes();

    DummySpikePlaybackBridge bridge;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("spikePlaybackBridge"), &bridge);
    const QString spike_window_path =
        QDir(QString::fromUtf8(SHATV_SOURCE_DIR)).filePath("src/ui/qml_spike/qml/SpikeWindow.qml");

    engine.load(QUrl::fromLocalFile(spike_window_path));

    QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(QString("Failed to load %1").arg(spike_window_path)));
    auto *root = engine.rootObjects().constFirst();
    QVERIFY(root != nullptr);
    auto *video_item = root->findChild<QObject *>(QStringLiteral("mpvVideoItem"));
    QVERIFY(video_item != nullptr);
    QVERIFY(video_item->property("ready").isValid());
    QCOMPARE(video_item->property("ready").toBool(), false);
}

void MpvVideoItemSpikeTest::qml_spike_window_marks_video_item_ready_after_backend_attach() {
    ConfigureSpikeGraphicsBackend();
    shatv::ui::qml_spike::RegisterQmlVideoTypes();

    DummySpikePlaybackBridge bridge;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("spikePlaybackBridge"), &bridge);
    const QString spike_window_path =
        QDir(QString::fromUtf8(SHATV_SOURCE_DIR)).filePath("src/ui/qml_spike/qml/SpikeWindow.qml");

    engine.load(QUrl::fromLocalFile(spike_window_path));

    QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(QString("Failed to load %1").arg(spike_window_path)));
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    auto *video_item =
        qobject_cast<shatv::ui::qml_spike::MpvVideoItem *>(window->findChild<QObject *>(QStringLiteral("mpvVideoItem")));
    QVERIFY(video_item != nullptr);
    QCOMPARE(video_item->ready(), false);

    window->show();
    QTRY_VERIFY(window->rendererInterface()->graphicsApi() != QSGRendererInterface::Unknown);
    if (window->rendererInterface()->graphicsApi() != QSGRendererInterface::OpenGL) {
        QSKIP("Current platform plugin does not expose an OpenGL Qt Quick scene graph backend");
    }

    shatv::player::MpvPlayerBackend backend;
    video_item->SetBackend(&backend);

    QTRY_VERIFY2(video_item->ready(), "MpvVideoItem did not create an OpenGL render context after backend attach");
}

}  // namespace

int main(int argc, char *argv[]) {
    ConfigureSpikeGraphicsBackend();
    QGuiApplication app(argc, argv);
    MpvVideoItemSpikeTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "mpv_video_item_spike_test.moc"
