#version 450

layout(location = 0) in vec3 vFragColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = 0.5 * (vec2(1.0) + vFragColor.xy);
    outColor = vec4(uv.x, 0.0, uv.y, 1.0);
}
