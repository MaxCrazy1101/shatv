#include <clocale>

#include <QDir>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>

#include "app/launch_options.h"
#include "application/player_controller.h"
#include "domain/playback_state.h"
#include "player/mpv_player_backend.h"

#include "ui/qml_spike/mpv_video_item.h"
#include "ui/qml_spike/spike_playback_bridge.h"

namespace {

QString GraphicsApiName(QSGRendererInterface::GraphicsApi api) {
    switch (api) {
        case QSGRendererInterface::Unknown:
            return QStringLiteral("unknown");
        case QSGRendererInterface::Software:
            return QStringLiteral("software");
        case QSGRendererInterface::OpenGL:
            return QStringLiteral("opengl");
        case QSGRendererInterface::Direct3D11:
            return QStringLiteral("d3d11");
        case QSGRendererInterface::Vulkan:
            return QStringLiteral("vulkan");
        case QSGRendererInterface::Metal:
            return QStringLiteral("metal");
        case QSGRendererInterface::Null:
            return QStringLiteral("null");
        case QSGRendererInterface::Direct3D12:
            return QStringLiteral("d3d12");
        case QSGRendererInterface::OpenVG:
            return QStringLiteral("openvg");
    }
    return QStringLiteral("unknown");
}

}  // namespace

int main(int argc, char *argv[]) {
    // Phase 1 spike 先钉住 OpenGL 和 basic render loop，单独验证视频承载进入 Qt Quick 场景。
    qputenv("QSG_RENDER_LOOP", "basic");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    // 先和主程序保持一致，避免后续接 mpv 时再引入数值本地化差异。
    std::setlocale(LC_NUMERIC, "C");

    shatv::ui::qml_spike::RegisterQmlVideoTypes();
    const shatv::app::LaunchOptions options = shatv::app::ParseLaunchOptions(app.arguments());
    const auto startup_channel =
        shatv::app::BuildStartupChannel(options, qEnvironmentVariable("SHATV_SMOKE_MEDIA"), QDir::currentPath());

    shatv::player::MpvPlayerBackend backend;
    shatv::application::PlayerController controller(&backend);
    shatv::ui::qml_spike::SpikePlaybackBridge bridge;
    bridge.SetController(&controller);
    bridge.SetStartupChannel(startup_channel);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("spikePlaybackBridge", &bridge);
    engine.loadFromModule("ShaTV.Spike", "SpikeWindow");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    auto *root = engine.rootObjects().constFirst();
    auto *window = qobject_cast<QQuickWindow *>(root);
    if (window == nullptr) {
        return 1;
    }
    auto *video_item = root->findChild<shatv::ui::qml_spike::MpvVideoItem *>(QStringLiteral("mpvVideoItem"));
    if (video_item == nullptr) {
        return 1;
    }

    video_item->SetBackend(&backend);
    bridge.SetVideoItem(video_item);

    QObject::connect(video_item, &shatv::ui::qml_spike::MpvVideoItem::readyChanged, &app, [video_item]() {
        qInfo().noquote() << QString("ShaTV QML spike video_ready=%1").arg(video_item->ready() ? "true" : "false");
    });
    QObject::connect(&controller, &shatv::application::PlayerController::PlaybackSnapshotChanged, &app,
                     [](const shatv::domain::PlayerSnapshot &snapshot) {
                         qInfo().noquote()
                             << QString("ShaTV QML spike snapshot state=%1 channel=%2 message=%3")
                                    .arg(shatv::domain::PlaybackStateToken(snapshot.state), snapshot.channel_id,
                                         snapshot.message);
                     });
    QTimer::singleShot(300, &app, [window, video_item]() {
        qInfo().noquote()
            << QString("ShaTV QML spike graphics_api=%1 video_ready=%2")
                   .arg(GraphicsApiName(window->rendererInterface()->graphicsApi()),
                        video_item->ready() ? QStringLiteral("true") : QStringLiteral("false"));
    });

    if (startup_channel.has_value()) {
        qInfo().noquote() << QString("ShaTV QML spike startup_media=%1").arg(startup_channel->url.toString());
        controller.PlayChannel(*startup_channel);
    } else {
        qInfo().noquote() << "ShaTV QML spike startup_media=<none>";
    }

    return app.exec();
}
