#version 450
layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
} cameraUBO;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 outColor;
void main() {
    gl_Position = cameraUBO.viewProj * vec4(inPosition, 1.0);

    outColor = inColor;
}