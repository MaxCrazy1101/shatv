#include "ui/qml_spike/mpv_video_item.h"

#include <QMetaObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFramebufferObjectFormat>
#include <QOpenGLFunctions>
#include <QtQml/qqml.h>

#include "player/mpv_player_backend.h"

namespace shatv::ui::qml_spike {

class MpvVideoRenderer final : public QQuickFramebufferObject::Renderer {
   public:
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size.expandedTo(QSize(1, 1)), format);
    }

    void synchronize(QQuickFramebufferObject *item) override {
        auto *video_item = static_cast<MpvVideoItem *>(item);
        item_ = video_item;
        backend_ = video_item->Backend();
    }

    void render() override {
        auto *context = QOpenGLContext::currentContext();
        if (context != nullptr) {
            auto *functions = context->functions();
            functions->glViewport(0, 0, framebufferObject()->width(), framebufferObject()->height());
            functions->glClearColor(0.04f, 0.08f, 0.13f, 1.0f);
            functions->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        if (backend_ != nullptr) {
            backend_->InitializeRenderContext();
            backend_->RenderFrame(framebufferObject()->handle(), framebufferObject()->width(),
                                  framebufferObject()->height(), 1.0);
            if (item_ != nullptr) {
                item_->UpdateReadyFromRenderThread(backend_->HasRenderContext());
            }
            return;
        }

        if (item_ != nullptr) {
            item_->UpdateReadyFromRenderThread(false);
        }
    }

   private:
    QPointer<shatv::player::MpvPlayerBackend> backend_;
    QPointer<MpvVideoItem> item_;
};

MpvVideoItem::MpvVideoItem(QQuickItem *parent) : QQuickFramebufferObject(parent) {
    setMirrorVertically(true);
}

MpvVideoItem::~MpvVideoItem() {
    if (backend_ != nullptr) {
        backend_->DetachRenderHost();
    }
}

bool MpvVideoItem::ready() const {
    return ready_;
}

void MpvVideoItem::SetBackend(shatv::player::MpvPlayerBackend *backend) {
    if (backend_ == backend) {
        return;
    }

    if (backend_ != nullptr) {
        backend_->DetachRenderHost();
    }

    backend_ = backend;
    ready_ = false;

    if (backend_ != nullptr) {
        backend_->AttachRenderHost(this);
    }

    emit readyChanged();
    update();
}

shatv::player::MpvPlayerBackend *MpvVideoItem::Backend() const {
    return backend_;
}

QQuickFramebufferObject::Renderer *MpvVideoItem::createRenderer() const {
    return new MpvVideoRenderer();
}

QOpenGLContext *MpvVideoItem::CurrentContext() const {
    return QOpenGLContext::currentContext();
}

void MpvVideoItem::MakeCurrent() {
    // QQuickFramebufferObject 由 Qt Quick render pass 驱动，这里不额外切换上下文。
}

void MpvVideoItem::DoneCurrent() {
    // Spike 阶段保持与 MakeCurrent 对称的空实现，避免额外引入 surface 管理。
}

void MpvVideoItem::RequestUpdate() {
    QMetaObject::invokeMethod(this, [this]() { update(); }, Qt::QueuedConnection);
}

void MpvVideoItem::ApplyReady(bool ready) {
    if (ready_ == ready) {
        return;
    }

    ready_ = ready;
    emit readyChanged();
}

void MpvVideoItem::UpdateReadyFromRenderThread(bool ready) {
    QMetaObject::invokeMethod(this, [this, ready]() { ApplyReady(ready); }, Qt::QueuedConnection);
}

void RegisterQmlVideoTypes() {
    static bool registered = false;
    if (registered) {
        return;
    }

    // 主线和 spike 共用同一个视频项注册入口，避免 QWidget/QML 两套视频承载继续分叉。
    qmlRegisterModule("ShaTV.Video", 1, 0);
    qmlRegisterType<MpvVideoItem>("ShaTV.Video", 1, 0, "MpvVideoItem");
    registered = true;
}

}  // namespace shatv::ui::qml_spike
