#version 450

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
} cameraUBO;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 vWorldPos;    // world-space now
layout(location = 1) out vec3 vWorldNormal;
layout(location = 2) out vec2 vTexCoord;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    vWorldPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(pc.model)));
    vWorldNormal = normalize(normalMatrix * inNormal);

    vTexCoord = inTexCoord;
    gl_Position = cameraUBO.viewProj * worldPos;
}