#include "player/mpv_player_backend.h"

#include <algorithm>

#include <QMetaObject>
#include <QOpenGLContext>
#include <QString>

#include <mpv/render_gl.h>

#include "player/mpv_render_widget.h"

namespace shatv::player {

namespace {

bool SupportsHttpHeaders(const QUrl &url) {
    if (!url.isValid() || url.isLocalFile()) {
        return false;
    }

    const QString scheme = url.scheme().toLower();
    return scheme == "http" || scheme == "https";
}

}  // namespace

MpvPlayerBackend::MpvPlayerBackend(QObject *parent) : PlayerBackend(parent) {
    InitializeMpv();
}

MpvPlayerBackend::~MpvPlayerBackend() {
    if (render_context_ != nullptr) {
        mpv_render_context_set_update_callback(render_context_, nullptr, nullptr);
        if (render_widget_ != nullptr && render_widget_->context() != nullptr) {
            render_widget_->makeCurrent();
            mpv_render_context_free(render_context_);
            render_widget_->doneCurrent();
        } else {
            mpv_render_context_free(render_context_);
        }
        render_context_ = nullptr;
    }

    if (handle_ != nullptr) {
        mpv_set_wakeup_callback(handle_, nullptr, nullptr);
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
    }
}

void MpvPlayerBackend::AttachRenderWidget(MpvRenderWidget *render_widget) {
    render_widget_ = render_widget;
}

void MpvPlayerBackend::DetachRenderWidget() {
    render_widget_ = nullptr;
}

void MpvPlayerBackend::InitializeRenderContext() {
    if (handle_ == nullptr || render_context_ != nullptr || render_widget_ == nullptr) {
        return;
    }

    auto *context = QOpenGLContext::currentContext();
    if (context == nullptr) {
        EmitSnapshot(domain::PlaybackState::kError, "mpv render context failed: no current OpenGL context");
        return;
    }

    mpv_opengl_init_params gl_init{
        .get_proc_address = &MpvPlayerBackend::GetProcAddress,
        .get_proc_address_ctx = context,
    };

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    const int result = mpv_render_context_create(&render_context_, handle_, params);
    if (result < 0) {
        render_context_ = nullptr;
        EmitSnapshot(domain::PlaybackState::kError,
                     QString("mpv render context create failed: %1").arg(mpv_error_string(result)));
        return;
    }

    mpv_render_context_set_update_callback(render_context_, &MpvPlayerBackend::OnRenderUpdate, this);

    if (pending_load_) {
        const domain::Channel channel = pending_channel_;
        pending_load_ = false;
        pending_channel_ = {};
        QMetaObject::invokeMethod(this, [this, channel]() { LoadInternal(channel); }, Qt::QueuedConnection);
    }
}

void MpvPlayerBackend::Load(const domain::Channel &channel) {
    current_channel_ = channel;
    EmitSnapshot(domain::PlaybackState::kLoading, QString("Loading %1").arg(channel.name));

    if (handle_ == nullptr) {
        EmitSnapshot(domain::PlaybackState::kError, "mpv backend initialization failed");
        return;
    }

    if (render_context_ == nullptr) {
        pending_load_ = true;
        pending_channel_ = channel;
        return;
    }

    LoadInternal(channel);
}

void MpvPlayerBackend::Play() {
    if (current_channel_.id.isEmpty()) {
        return;
    }

    if (handle_ == nullptr) {
        EmitSnapshot(domain::PlaybackState::kError, "mpv backend initialization failed");
        return;
    }

    mpv_set_property_string(handle_, "pause", "no");
    EmitSnapshot(domain::PlaybackState::kPlaying, QString("Playing %1").arg(current_channel_.name));
}

void MpvPlayerBackend::Pause() {
    if (current_channel_.id.isEmpty()) {
        return;
    }

    if (handle_ == nullptr) {
        EmitSnapshot(domain::PlaybackState::kError, "mpv backend initialization failed");
        return;
    }

    mpv_set_property_string(handle_, "pause", "yes");
    EmitSnapshot(domain::PlaybackState::kPaused, QString("Paused %1").arg(current_channel_.name));
}

void MpvPlayerBackend::Stop() {
    if (handle_ != nullptr) {
        const char *command[] = {"stop", nullptr};
        mpv_command(handle_, command);
    }

    current_channel_ = {};
    pending_channel_ = {};
    pending_load_ = false;
    EmitSnapshot(domain::PlaybackState::kIdle, "Stopped");
}

void MpvPlayerBackend::SetVolume(int volume) {
    volume_ = std::clamp(volume, 0, 100);
    if (handle_ != nullptr) {
        const QByteArray volume_value = QByteArray::number(volume_);
        mpv_set_property_string(handle_, "volume", volume_value.constData());
    }

    domain::PlayerSnapshot snapshot = current_snapshot_;
    snapshot.channel_id = current_channel_.id;
    snapshot.channel_name = current_channel_.name;
    snapshot.volume = volume_;
    snapshot.muted = muted_;
    snapshot.message = QString("Volume %1").arg(volume_);
    EmitSnapshot(snapshot);
}

void MpvPlayerBackend::SetMuted(bool muted) {
    muted_ = muted;
    if (handle_ != nullptr) {
        mpv_set_property_string(handle_, "mute", muted_ ? "yes" : "no");
    }

    domain::PlayerSnapshot snapshot = current_snapshot_;
    snapshot.channel_id = current_channel_.id;
    snapshot.channel_name = current_channel_.name;
    snapshot.volume = volume_;
    snapshot.muted = muted_;
    snapshot.message = muted_ ? "Muted" : "Unmuted";
    EmitSnapshot(snapshot);
}

void MpvPlayerBackend::SetNetworkUserAgent(const QString &user_agent) {
    user_agent_ = user_agent;
}

bool MpvPlayerBackend::RenderFrame(int framebuffer_object, int width, int height, double device_pixel_ratio) {
    if (render_context_ == nullptr) {
        return false;
    }

    mpv_render_context_update(render_context_);

    mpv_opengl_fbo fbo{
        .fbo = framebuffer_object,
        .w = std::max(1, static_cast<int>(width * device_pixel_ratio)),
        .h = std::max(1, static_cast<int>(height * device_pixel_ratio)),
        .internal_format = 0,
    };
    int flip_y = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    return mpv_render_context_render(render_context_, params) >= 0;
}

void MpvPlayerBackend::EmitSnapshot(const domain::PlayerSnapshot &snapshot) {
    current_snapshot_ = snapshot;
    emit SnapshotChanged(current_snapshot_);
}

void MpvPlayerBackend::EmitSnapshot(domain::PlaybackState state, const QString &message) {
    domain::PlayerSnapshot snapshot;
    snapshot.state = state;
    snapshot.channel_id = current_channel_.id;
    snapshot.channel_name = current_channel_.name;
    snapshot.message = message;
    snapshot.volume = volume_;
    snapshot.muted = muted_;

    EmitSnapshot(snapshot);
}

void MpvPlayerBackend::InitializeMpv() {
    handle_ = mpv_create();
    if (handle_ == nullptr) {
        return;
    }

    mpv_set_option_string(handle_, "config", "no");
    mpv_set_option_string(handle_, "terminal", "no");
    mpv_set_option_string(handle_, "vo", "libmpv");
    mpv_set_option_string(handle_, "hwdec", "no");
    mpv_set_option_string(handle_, "keep-open", "yes");

    if (mpv_initialize(handle_) < 0) {
        mpv_terminate_destroy(handle_);
        handle_ = nullptr;
        return;
    }

    mpv_set_wakeup_callback(handle_, &MpvPlayerBackend::OnWakeup, this);
    mpv_observe_property(handle_, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(handle_, 0, "idle-active", MPV_FORMAT_FLAG);
    mpv_observe_property(handle_, 0, "eof-reached", MPV_FORMAT_FLAG);
}

void MpvPlayerBackend::LoadInternal(const domain::Channel &channel) {
    if (handle_ == nullptr) {
        EmitSnapshot(domain::PlaybackState::kError, "mpv backend initialization failed");
        return;
    }

    current_channel_ = channel;

    mpv_set_property_string(handle_, "pause", "no");
    mpv_set_property_string(handle_, "mute", muted_ ? "yes" : "no");

    const QByteArray volume_value = QByteArray::number(volume_);
    mpv_set_property_string(handle_, "volume", volume_value.constData());

    const QByteArray header_fields =
        (!user_agent_.isEmpty() && SupportsHttpHeaders(channel.url))
            ? QString("User-Agent: %1").arg(user_agent_).toUtf8()
            : QByteArray();
    mpv_set_property_string(handle_, "http-header-fields", header_fields.constData());

    const QByteArray url = channel.url.toString().toUtf8();
    const char *command[] = {"loadfile", url.constData(), "replace", nullptr};
    const int result = mpv_command(handle_, command);
    if (result < 0) {
        EmitSnapshot(domain::PlaybackState::kError,
                     QString("mpv loadfile failed: %1").arg(mpv_error_string(result)));
    }
}

void MpvPlayerBackend::DrainEvents() {
    if (handle_ == nullptr) {
        return;
    }

    while (true) {
        mpv_event *event = mpv_wait_event(handle_, 0.0);
        if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
            break;
        }

        HandleEvent(event);
    }
}

void MpvPlayerBackend::HandleEvent(mpv_event *event) {
    switch (event->event_id) {
        case MPV_EVENT_FILE_LOADED: {
            domain::PlayerSnapshot snapshot;
            snapshot.channel_id = current_channel_.id;
            snapshot.channel_name = current_channel_.name;
            snapshot.volume = volume_;
            snapshot.muted = muted_;
            event_adapter_.ApplyFileLoaded(snapshot, current_channel_.name);
            EmitSnapshot(snapshot);
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            auto *property = static_cast<mpv_event_property *>(event->data);
            if (property == nullptr || property->name == nullptr) {
                break;
            }

            if (std::string_view(property->name) == "idle-active" && property->format == MPV_FORMAT_FLAG &&
                property->data != nullptr) {
                domain::PlayerSnapshot snapshot = current_snapshot_;
                snapshot.channel_id = current_channel_.id;
                snapshot.channel_name = current_channel_.name;
                snapshot.volume = volume_;
                snapshot.muted = muted_;
                const int idle_active = *static_cast<int *>(property->data);
                event_adapter_.ApplyIdleActive(snapshot, idle_active != 0);
                EmitSnapshot(snapshot);
                break;
            }

            if (std::string_view(property->name) == "eof-reached" && property->format == MPV_FORMAT_FLAG &&
                property->data != nullptr) {
                domain::PlayerSnapshot snapshot = current_snapshot_;
                snapshot.channel_id = current_channel_.id;
                snapshot.channel_name = current_channel_.name;
                snapshot.volume = volume_;
                snapshot.muted = muted_;
                const int eof_reached = *static_cast<int *>(property->data);
                event_adapter_.ApplyEofReached(snapshot, eof_reached != 0);
                EmitSnapshot(snapshot);
                break;
            }

            if (std::string_view(property->name) == "pause" && property->format == MPV_FORMAT_FLAG &&
                property->data != nullptr) {
                domain::PlayerSnapshot snapshot = current_snapshot_;
                snapshot.channel_id = current_channel_.id;
                snapshot.channel_name = current_channel_.name;
                snapshot.volume = volume_;
                snapshot.muted = muted_;
                const int paused = *static_cast<int *>(property->data);
                event_adapter_.ApplyPauseChanged(snapshot, paused != 0);
                EmitSnapshot(snapshot);
            }
            break;
        }
        case MPV_EVENT_END_FILE: {
            auto *end_file = static_cast<mpv_event_end_file *>(event->data);
            if (end_file != nullptr && end_file->reason == MPV_END_FILE_REASON_ERROR) {
                domain::PlayerSnapshot snapshot;
                snapshot.channel_id = current_channel_.id;
                snapshot.channel_name = current_channel_.name;
                snapshot.volume = volume_;
                snapshot.muted = muted_;
                event_adapter_.ApplyEndFileError(
                    snapshot, QString("Playback error: %1").arg(mpv_error_string(end_file->error)));
                EmitSnapshot(snapshot);
            } else if (end_file != nullptr && end_file->reason == MPV_END_FILE_REASON_EOF) {
                domain::PlayerSnapshot snapshot;
                snapshot.channel_id = current_channel_.id;
                snapshot.channel_name = current_channel_.name;
                snapshot.volume = volume_;
                snapshot.muted = muted_;
                event_adapter_.ApplyEndFileEof(snapshot);
                EmitSnapshot(snapshot);
            } else if (end_file != nullptr && end_file->reason == MPV_END_FILE_REASON_STOP) {
                EmitSnapshot(domain::PlaybackState::kIdle, "Stopped");
            }
            break;
        }
        default:
            break;
    }
}

void MpvPlayerBackend::RequestRenderUpdate() {
    if (render_widget_ != nullptr) {
        render_widget_->update();
    }
}

void *MpvPlayerBackend::GetProcAddress(void *ctx, const char *name) {
    auto *context = static_cast<QOpenGLContext *>(ctx);
    if (context == nullptr) {
        context = QOpenGLContext::currentContext();
    }

    if (context == nullptr) {
        return nullptr;
    }

    return reinterpret_cast<void *>(context->getProcAddress(name));
}

void MpvPlayerBackend::OnWakeup(void *ctx) {
    auto *self = static_cast<MpvPlayerBackend *>(ctx);
    if (self == nullptr) {
        return;
    }

    QMetaObject::invokeMethod(self, [self]() { self->DrainEvents(); }, Qt::QueuedConnection);
}

void MpvPlayerBackend::OnRenderUpdate(void *ctx) {
    auto *self = static_cast<MpvPlayerBackend *>(ctx);
    if (self == nullptr) {
        return;
    }

    QMetaObject::invokeMethod(self, [self]() { self->RequestRenderUpdate(); }, Qt::QueuedConnection);
}

}  // namespace shatv::player
