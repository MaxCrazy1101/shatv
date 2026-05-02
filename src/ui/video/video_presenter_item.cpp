#include "ui/video/video_presenter_item.h"

#include <array>
#include <memory>

#include <QFile>
#include <QMatrix4x4>
#include <QMetaObject>
#include <QQuickWindow>
#include <QtQml/qqml.h>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

#include "player/ffmpeg_player_backend.h"

namespace shatv::ui::video {

namespace {

struct Vertex {
    float x;
    float y;
    float u;
    float v;
};

constexpr std::array<Vertex, 4> kQuadVertices{{
    {-1.0F, -1.0F, 0.0F, 1.0F},
    {1.0F, -1.0F, 1.0F, 1.0F},
    {-1.0F, 1.0F, 0.0F, 0.0F},
    {1.0F, 1.0F, 1.0F, 0.0F},
}};

QShader LoadShader(const QString &path) {
    QFile shader_file(path);
    if (!shader_file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QShader::fromSerialized(shader_file.readAll());
}

class VideoPresenterRenderer final : public QQuickRhiItemRenderer {
   public:
    void initialize(QRhiCommandBuffer *cb) override {
        ResetPipeline();
        vertex_buffer_.reset(rhi()->newBuffer(QRhiBuffer::Immutable,
                                              QRhiBuffer::VertexBuffer,
                                              static_cast<quint32>(sizeof(Vertex) * kQuadVertices.size())));
        vertex_buffer_->create();

        vertex_uniform_buffer_.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        vertex_uniform_buffer_->create();
        fragment_uniform_buffer_.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));
        fragment_uniform_buffer_->create();
        sampler_.reset(rhi()->newSampler(QRhiSampler::Linear,
                                         QRhiSampler::Linear,
                                         QRhiSampler::None,
                                         QRhiSampler::ClampToEdge,
                                         QRhiSampler::ClampToEdge));
        sampler_->create();

        QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();
        updates->uploadStaticBuffer(vertex_buffer_.get(), kQuadVertices.data());
        cb->resourceUpdate(updates);
    }

    void synchronize(QQuickRhiItem *item) override {
        auto *video_item = static_cast<VideoPresenterItem *>(item);
        media::video::VideoFrame frame;
        if (video_item->TakePendingFrame(&frame)) {
            pending_frame_ = std::move(frame);
            has_pending_frame_ = true;
        }
    }

    void render(QRhiCommandBuffer *cb) override {
        QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();
        const bool has_frame = ApplyPendingFrame(updates);

        QMatrix4x4 mvp = rhi()->clipSpaceCorrMatrix();
        updates->updateDynamicBuffer(vertex_uniform_buffer_.get(), 0, 64, mvp.constData());
        const std::array<float, 4> fragment_ubo{{1.0F, 0.0F, 0.0F, 0.0F}};
        updates->updateDynamicBuffer(fragment_uniform_buffer_.get(), 0, 16, fragment_ubo.data());

        cb->beginPass(renderTarget(), QColor::fromRgbF(0.0, 0.0, 0.0, 1.0), {1.0F, 0}, updates);
        if (has_frame && EnsurePipeline()) {
            cb->setGraphicsPipeline(pipeline_.get());
            cb->setViewport(QRhiViewport(0, 0, renderTarget()->pixelSize().width(), renderTarget()->pixelSize().height()));
            cb->setShaderResources(shader_resource_bindings_.get());
            const QRhiCommandBuffer::VertexInput vertex_binding(vertex_buffer_.get(), 0);
            cb->setVertexInput(0, 1, &vertex_binding);
            cb->draw(4);
        }
        cb->endPass();
    }

   private:
    bool ApplyPendingFrame(QRhiResourceUpdateBatch *updates) {
        if (has_pending_frame_) {
            current_frame_ = std::move(pending_frame_);
            has_pending_frame_ = false;
            EnsureTextures(current_frame_);
            UploadPlane(updates, y_texture_.get(), current_frame_.y_plane, current_frame_.size.width(),
                        current_frame_.size.height());
            UploadPlane(updates, u_texture_.get(), current_frame_.u_plane, (current_frame_.size.width() + 1) / 2,
                        (current_frame_.size.height() + 1) / 2);
            UploadPlane(updates, v_texture_.get(), current_frame_.v_plane, (current_frame_.size.width() + 1) / 2,
                        (current_frame_.size.height() + 1) / 2);
            return true;
        }

        return current_frame_.size.isValid();
    }

    void EnsureTextures(const media::video::VideoFrame &frame) {
        const QSize y_size = frame.size;
        const QSize uv_size((frame.size.width() + 1) / 2, (frame.size.height() + 1) / 2);
        if (y_texture_ != nullptr && y_texture_->pixelSize() == y_size) {
            return;
        }

        y_texture_.reset(rhi()->newTexture(QRhiTexture::R8, y_size));
        u_texture_.reset(rhi()->newTexture(QRhiTexture::R8, uv_size));
        v_texture_.reset(rhi()->newTexture(QRhiTexture::R8, uv_size));
        y_texture_->create();
        u_texture_->create();
        v_texture_->create();
        ResetPipeline();
    }

    void UploadPlane(QRhiResourceUpdateBatch *updates,
                     QRhiTexture *texture,
                     const QByteArray &plane,
                     int width,
                     int height) {
        QRhiTextureSubresourceUploadDescription subresource(plane);
        subresource.setDataStride(static_cast<quint32>(width));
        subresource.setSourceSize(QSize(width, height));
        updates->uploadTexture(texture, QRhiTextureUploadDescription(QRhiTextureUploadEntry(0, 0, subresource)));
    }

    bool EnsurePipeline() {
        if (pipeline_ != nullptr) {
            return true;
        }
        if (y_texture_ == nullptr || u_texture_ == nullptr || v_texture_ == nullptr) {
            return false;
        }

        shader_resource_bindings_.reset(rhi()->newShaderResourceBindings());
        shader_resource_bindings_->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, vertex_uniform_buffer_.get()),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, y_texture_.get(), sampler_.get()),
            QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, u_texture_.get(), sampler_.get()),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, v_texture_.get(), sampler_.get()),
            QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage, fragment_uniform_buffer_.get()),
        });
        if (!shader_resource_bindings_->create()) {
            return false;
        }

        QShader vertex_shader = LoadShader(QStringLiteral(":/shatv/shaders/ui/video/shaders/video_present.vert.qsb"));
        QShader fragment_shader = LoadShader(QStringLiteral(":/shatv/shaders/ui/video/shaders/video_present.frag.qsb"));
        if (!vertex_shader.isValid() || !fragment_shader.isValid()) {
            return false;
        }

        QRhiVertexInputLayout input_layout;
        input_layout.setBindings({QRhiVertexInputBinding(sizeof(Vertex))});
        input_layout.setAttributes({
            QRhiVertexInputAttribute(0, 0, QRhiVertexInputAttribute::Float2, 0),
            QRhiVertexInputAttribute(0, 1, QRhiVertexInputAttribute::Float2, sizeof(float) * 2),
        });

        pipeline_.reset(rhi()->newGraphicsPipeline());
        pipeline_->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        pipeline_->setShaderStages({
            QRhiShaderStage(QRhiShaderStage::Vertex, vertex_shader),
            QRhiShaderStage(QRhiShaderStage::Fragment, fragment_shader),
        });
        pipeline_->setVertexInputLayout(input_layout);
        pipeline_->setShaderResourceBindings(shader_resource_bindings_.get());
        pipeline_->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        return pipeline_->create();
    }

    void ResetPipeline() {
        pipeline_.reset();
        shader_resource_bindings_.reset();
    }

    media::video::VideoFrame current_frame_;
    media::video::VideoFrame pending_frame_;
    bool has_pending_frame_ = false;
    std::unique_ptr<QRhiBuffer> vertex_buffer_;
    std::unique_ptr<QRhiBuffer> vertex_uniform_buffer_;
    std::unique_ptr<QRhiBuffer> fragment_uniform_buffer_;
    std::unique_ptr<QRhiSampler> sampler_;
    std::unique_ptr<QRhiTexture> y_texture_;
    std::unique_ptr<QRhiTexture> u_texture_;
    std::unique_ptr<QRhiTexture> v_texture_;
    std::unique_ptr<QRhiShaderResourceBindings> shader_resource_bindings_;
    std::unique_ptr<QRhiGraphicsPipeline> pipeline_;
};

}  // namespace

VideoPresenterItem::VideoPresenterItem(QQuickItem *parent) : QQuickRhiItem(parent) {
    setMirrorVertically(false);
}

VideoPresenterItem::~VideoPresenterItem() {
    if (backend_ != nullptr) {
        backend_->DetachVideoSink(this);
    }
}

bool VideoPresenterItem::ready() const {
    return ready_;
}

void VideoPresenterItem::SetBackend(shatv::player::FfmpegPlayerBackend *backend) {
    if (backend_ == backend) {
        return;
    }

    if (backend_ != nullptr) {
        backend_->DetachVideoSink(this);
    }

    backend_ = backend;
    ApplyReady(false);

    if (backend_ != nullptr) {
        backend_->AttachVideoSink(this);
    }

    update();
}

shatv::player::FfmpegPlayerBackend *VideoPresenterItem::Backend() const {
    return backend_;
}

QQuickRhiItemRenderer *VideoPresenterItem::createRenderer() {
    return new VideoPresenterRenderer();
}

void VideoPresenterItem::PresentVideoFrame(const media::video::VideoFrame &frame) {
    QMetaObject::invokeMethod(this, [this, frame]() {
        pending_frame_ = frame;
        has_pending_frame_ = true;
        ApplyReady(true);
        update();
    }, Qt::QueuedConnection);
}

bool VideoPresenterItem::TakePendingFrame(media::video::VideoFrame *frame) {
    if (frame == nullptr || !has_pending_frame_) {
        return false;
    }

    *frame = std::move(pending_frame_);
    has_pending_frame_ = false;
    return true;
}

void VideoPresenterItem::ApplyReady(bool ready) {
    if (ready_ == ready) {
        return;
    }

    ready_ = ready;
    emit readyChanged();
}

void RegisterQmlVideoTypes() {
    qmlRegisterType<VideoPresenterItem>("ShaTV.Video", 1, 0, "VideoPresenterItem");
}

}  // namespace shatv::ui::video
