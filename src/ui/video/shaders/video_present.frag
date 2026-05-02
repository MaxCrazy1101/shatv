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

    // BT.709 limited range. Later stages will switch matrix/range from AVFrame metadata.
    y = 1.16438356 * (y - 0.0625);
    float r = y + 1.79274107 * v;
    float g = y - 0.21324861 * u - 0.53290933 * v;
    float b = y + 2.11240179 * u;

    fragColor = vec4(clamp(vec3(r, g, b), 0.0, 1.0), fragment_ubo.opacity);
}
