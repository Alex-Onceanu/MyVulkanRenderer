#version 450

layout(location = 0) in vec3 vFragColor;
layout(location = 1) in float vTime;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 uv = 0.5 * (vec2(1.0) + vFragColor.xy);
    float t = 0.5 + 0.5 * sin(vTime);
    outColor = vec4(uv.x, 0.0, uv.y, 1.0) * t + (1.0 - t) * vec4(uv.y, uv.x, 1.0 - uv.x * uv.y, 1.0);
}