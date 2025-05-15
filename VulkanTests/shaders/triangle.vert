#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aClr;

layout(binding = 0) uniform UniformBufferObject {
    float uTime;
    vec3 uClr;
} ubo;

layout(location = 0) out vec3 vFragColor;
layout(location = 1) out float vTime;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vFragColor = vec3(aPos, 0.0);
    vTime = ubo.uTime;
}
