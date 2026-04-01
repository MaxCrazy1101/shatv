#pragma once

#include <mpv/client.h>
#include <mpv/render.h>

#include "player/mpv_event_adapter.h"
#include "player/player_backend.h"

namespace shatv::player {

class MpvRenderWidget;

class MpvPlayerBackend final : public PlayerBackend {
    Q_OBJECT

   public:
    explicit MpvPlayerBackend(QObject *parent = nullptr);
    ~MpvPlayerBackend() override;

    void AttachRenderWidget(MpvRenderWidget *render_widget);
    void DetachRenderWidget();
    void InitializeRenderContext();
    void Load(const domain::Channel &channel) override;
    void Play() override;
    void Pause() override;
    void Stop() override;
    void SetVolume(int volume) override;
    void SetMuted(bool muted) override;
    bool RenderFrame(int framebuffer_object, int width, int height, double device_pixel_ratio);

   private:
    void EmitSnapshot(const domain::PlayerSnapshot &snapshot);
    void EmitSnapshot(domain::PlaybackState state, const QString &message);
    void InitializeMpv();
    void LoadInternal(const domain::Channel &channel);
    void DrainEvents();
    void HandleEvent(mpv_event *event);
    void RequestRenderUpdate();

    static void *GetProcAddress(void *ctx, const char *name);
    static void OnWakeup(void *ctx);
    static void OnRenderUpdate(void *ctx);

    domain::Channel current_channel_;
    domain::Channel pending_channel_;
    domain::PlayerSnapshot current_snapshot_;
    int volume_ = 50;
    bool muted_ = false;
    bool pending_load_ = false;
    mpv_handle *handle_ = nullptr;
    mpv_render_context *render_context_ = nullptr;
    MpvRenderWidget *render_widget_ = nullptr;
    MpvEventAdapter event_adapter_;
};

}  // namespace shatv::player
