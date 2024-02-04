#version 450

layout(location = 0) in vec3 posL;
layout(location = 1) in vec3 colorIn;

layout(binding = 0) uniform UniformBufferObject {
    mat4 modelMat;
    mat4 viewMat;
    mat4 projMat;
} ubo;

layout(location = 0) out vec3 colorOut;

void main() {
    gl_Position = ubo.projMat * ubo.viewMat * ubo.modelMat * vec4(posL, 1.0f);
    gl_Position.y = -gl_Position.y; // Flip NDC-coord to matches with view-coord.

    colorOut = colorIn;
}