#pragma once

#include <QPointer>
#include <QQuickFramebufferObject>

#include "player/mpv_render_host.h"

namespace shatv::player {
class MpvPlayerBackend;
}

namespace shatv::ui::qml_spike {

class MpvVideoRenderer;

class MpvVideoItem : public QQuickFramebufferObject, public shatv::player::MpvRenderHost {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)

   public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    bool ready() const;
    void SetBackend(shatv::player::MpvPlayerBackend *backend);
    shatv::player::MpvPlayerBackend *Backend() const;
    Renderer *createRenderer() const override;

    QOpenGLContext *CurrentContext() const override;
    void MakeCurrent() override;
    void DoneCurrent() override;
    void RequestUpdate() override;

   signals:
    void readyChanged();

   private:
    void ApplyReady(bool ready);
    void UpdateReadyFromRenderThread(bool ready);

    QPointer<shatv::player::MpvPlayerBackend> backend_;
    bool ready_ = false;

    friend class MpvVideoRenderer;
};

void RegisterQmlVideoTypes();

}  // namespace shatv::ui::qml_spike
