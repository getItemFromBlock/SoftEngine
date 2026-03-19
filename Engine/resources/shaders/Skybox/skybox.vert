#version 450
layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
} cameraUBO;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 vTexCoord;

void main() {
    vTexCoord = inPosition;
    vec4 pos = cameraUBO.viewProj * vec4(inPosition, 1.0);
    gl_Position = pos.xyww;
}