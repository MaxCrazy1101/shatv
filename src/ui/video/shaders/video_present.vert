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
