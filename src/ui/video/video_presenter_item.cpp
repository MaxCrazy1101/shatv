#include "ui/video/video_presenter_item.h"

#include <QtQml/qqml.h>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>
#include <rhi/qshaderbaker.h>

#include <QMatrix4x4>
#include <QMetaObject>
#include <QQuickWindow>
#include <QThreadStorage>
#include <array>
#include <memory>

#include "app/logging.h"
#include "player/ffmpeg_player_backend.h"

namespace shatv::ui::video {

// NOLINTBEGIN(modernize-use-using) - QMatrix4x4 type alias for clarity
using Matrix4x4 = QMatrix4x4;
// NOLINTEND(modernize-use-using)

namespace {

using VideoAspectRatioMode = VideoPresenterItem::VideoAspectRatioMode;

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

// GLSL 440 source for YUV→RGB conversion (BT.709 limited range).
constexpr char kVertexShaderSource[] = R"(
#version 440
layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 v_texcoord;
layout(std140, binding = 0) uniform VertexUBO {
    mat4 mvp;
} vertex_ubo;
void main() {
    v_texcoord = texcoord;
    gl_Position = vertex_ubo.mvp * position;
}
)";

constexpr char kFragmentShaderSource[] = R"(
#version 440
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;
layout(binding = 1) uniform sampler2D y_tex;
layout(binding = 2) uniform sampler2D u_tex;
layout(binding = 3) uniform sampler2D v_tex;
layout(std140, binding = 4) uniform FragmentUBO {
    float opacity;
} fragment_ubo;
void main() {
    float y = texture(y_tex, v_texcoord).r;
    float u = texture(u_tex, v_texcoord).r - 0.5;
    float v = texture(v_tex, v_texcoord).r - 0.5;
    // BT.709 limited range.
    y = 1.16438356 * (y - 0.0625);
    float r = y + 1.79274107 * v;
    float g = y - 0.21324861 * u - 0.53290933 * v;
    float b = y + 2.11240179 * u;
    fragColor = vec4(clamp(vec3(r, g, b), 0.0, 1.0), fragment_ubo.opacity);
}
)";

QShader BakeShader(QShader::Stage stage, const char *source) {
    QShaderBaker baker;
    baker.setSourceString(source, stage);
    baker.setGeneratedShaders({
        {QShader::SpirvShader, QShaderVersion(100)},
        {QShader::GlslShader, QShaderVersion(100, QShaderVersion::GlslEs)},
        {QShader::GlslShader, QShaderVersion(120)},
        {QShader::HlslShader, QShaderVersion(50)},
        {QShader::MslShader, QShaderVersion(12)},
    });
    baker.setGeneratedShaderVariants({QShader::StandardShader});
    const QShader result = baker.bake();
    if (!result.isValid()) {
        qCWarning(app::log_video).noquote() << "Video shader bake failed reason=" << baker.errorMessage();
    } else {
        qCInfo(app::log_video) << "Video shader baked" << "stage=" << static_cast<int>(stage);
    }
    return result;
}

struct VideoShaders {
    QShader vertex;
    QShader fragment;
};

VideoShaders *ThreadLocalVideoShaders() {
    static QThreadStorage<VideoShaders *> shader_storage;
    if (!shader_storage.hasLocalData()) {
        shader_storage.setLocalData(new VideoShaders{
            BakeShader(QShader::VertexStage, kVertexShaderSource),
            BakeShader(QShader::FragmentStage, kFragmentShaderSource),
        });
    }
    return shader_storage.localData();
}

// Calculate aspect ratio correction scale factors.
// Returns (scale_x, scale_y) to apply to the video quad.
// - PreserveAspectRatio: fit inside viewport with letterbox/pillarbox
// - StretchToFill: no correction (1.0, 1.0)
// - CropToFill: fill viewport (may crop)
// - NativeSize: no correction (1.0, 1.0)
QVector2D CalculateAspectRatioScale(VideoAspectRatioMode mode, const QSize &frame_size, const QSize &viewport_size) {
    if (mode == VideoPresenterItem::StretchToFill || mode == VideoPresenterItem::NativeSize) {
        return {1.0F, 1.0F};
    }

    if (!frame_size.isValid() || !viewport_size.isValid() || frame_size.isEmpty() || viewport_size.isEmpty()) {
        return {1.0F, 1.0F};
    }

    const float frame_aspect = static_cast<float>(frame_size.width()) / frame_size.height();
    const float viewport_aspect = static_cast<float>(viewport_size.width()) / viewport_size.height();

    if (mode == VideoPresenterItem::PreserveAspectRatio) {
        if (frame_aspect > viewport_aspect) {
            // Frame is wider than viewport: scale Y to fit, X will have pillarbox
            return {1.0F, viewport_aspect / frame_aspect};
        }
        // Frame is taller than viewport: scale X to fit, Y will have letterbox
        return {frame_aspect / viewport_aspect, 1.0F};
    }

    if (mode == VideoPresenterItem::CropToFill) {
        if (frame_aspect > viewport_aspect) {
            // Frame is wider: scale Y to fill, X will crop
            return {frame_aspect / viewport_aspect, 1.0F};
        }
        // Frame is taller: scale X to fill, Y will crop
        return {1.0F, viewport_aspect / frame_aspect};
    }

    return {1.0F, 1.0F};
}

class VideoPresenterRenderer final : public QQuickRhiItemRenderer {
   public:
    void initialize(QRhiCommandBuffer *cb) override {
        VideoShaders *shaders = ThreadLocalVideoShaders();
        vertex_shader_ = shaders->vertex;
        fragment_shader_ = shaders->fragment;

        ResetPipeline();
        vertex_buffer_.reset(rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
                                              static_cast<quint32>(sizeof(Vertex) * kQuadVertices.size())));
        vertex_buffer_->create();

        vertex_uniform_buffer_.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        vertex_uniform_buffer_->create();
        fragment_uniform_buffer_.reset(rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16));
        fragment_uniform_buffer_->create();
        sampler_.reset(rhi()->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                         QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        sampler_->create();

        QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();
        updates->uploadStaticBuffer(vertex_buffer_.get(), kQuadVertices.data());
        cb->resourceUpdate(updates);
    }

    void synchronize(QQuickRhiItem *item) override {
        auto *video_item = static_cast<VideoPresenterItem *>(item);

        // Read aspect ratio mode from the item.
        aspect_ratio_mode_ = video_item->aspectRatioMode();

        media::video::VideoFrame frame;
        if (video_item->TakePendingFrame(&frame)) {
            pending_frame_ = std::move(frame);
            has_pending_frame_ = true;
        }
    }

    void render(QRhiCommandBuffer *cb) override {
        QRhiResourceUpdateBatch *updates = rhi()->nextResourceUpdateBatch();
        const bool has_frame = ApplyPendingFrame(updates);

        // Build MVP with aspect ratio correction.
        Matrix4x4 mvp = rhi()->clipSpaceCorrMatrix();
        const QSize viewport_size = renderTarget()->pixelSize();
        const QVector2D scale = CalculateAspectRatioScale(aspect_ratio_mode_, current_frame_.size, viewport_size);
        Matrix4x4 scale_matrix;
        scale_matrix.scale(scale.x(), scale.y(), 1.0F);
        mvp = scale_matrix * mvp;

        updates->updateDynamicBuffer(vertex_uniform_buffer_.get(), 0, 64, mvp.constData());
        const std::array<float, 4> fragment_ubo{{1.0F, 0.0F, 0.0F, 0.0F}};
        updates->updateDynamicBuffer(fragment_uniform_buffer_.get(), 0, 16, fragment_ubo.data());

        cb->beginPass(renderTarget(), QColor::fromRgbF(0.0, 0.0, 0.0, 1.0), {1.0F, 0}, updates);
        if (has_frame && EnsurePipeline()) {
            cb->setGraphicsPipeline(pipeline_.get());
            cb->setViewport(QRhiViewport(0, 0, viewport_size.width(), viewport_size.height()));
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

    void UploadPlane(QRhiResourceUpdateBatch *updates, QRhiTexture *texture, const QByteArray &plane, int width,
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
        if (!vertex_shader_.isValid() || !fragment_shader_.isValid()) {
            return false;
        }

        shader_resource_bindings_.reset(rhi()->newShaderResourceBindings());
        shader_resource_bindings_->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                     vertex_uniform_buffer_.get()),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, y_texture_.get(),
                                                      sampler_.get()),
            QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, u_texture_.get(),
                                                      sampler_.get()),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, v_texture_.get(),
                                                      sampler_.get()),
            QRhiShaderResourceBinding::uniformBuffer(4, QRhiShaderResourceBinding::FragmentStage,
                                                     fragment_uniform_buffer_.get()),
        });
        if (!shader_resource_bindings_->create()) {
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
            QRhiShaderStage(QRhiShaderStage::Vertex, vertex_shader_),
            QRhiShaderStage(QRhiShaderStage::Fragment, fragment_shader_),
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

    VideoAspectRatioMode aspect_ratio_mode_ = VideoPresenterItem::PreserveAspectRatio;
    QShader vertex_shader_;
    QShader fragment_shader_;
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

VideoPresenterItem::VideoAspectRatioMode VideoPresenterItem::aspectRatioMode() const {
    return aspect_ratio_mode_;
}

void VideoPresenterItem::setAspectRatioMode(VideoAspectRatioMode mode) {
    if (aspect_ratio_mode_ == mode) {
        return;
    }
    aspect_ratio_mode_ = mode;
    emit aspectRatioModeChanged();
    update();
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
    QMetaObject::invokeMethod(
        this,
        [this, frame]() {
            pending_frame_ = frame;
            has_pending_frame_ = true;
            ApplyReady(true);
            update();
        },
        Qt::QueuedConnection);
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
