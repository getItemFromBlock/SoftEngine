#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;

layout(location = 4) in vec4 instancePosition;
layout(location = 5) in vec4 instanceColor;

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
} cameraUBO;

layout(location = 0) out vec4 fragColor;

void main() {
    vec3 scaledPosition = inPosition * instancePosition.w;
    vec4 worldPos = vec4(scaledPosition + instancePosition.xyz, 1.0);
    gl_Position = cameraUBO.viewProj * worldPos;
    fragColor = instanceColor;
}