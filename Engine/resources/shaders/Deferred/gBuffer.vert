#version 450

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec3 cameraPos;
} cameraUBO;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) out mat3 vTBN;
layout(location = 6) out vec3 vTangentViewDir;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    vWorldPos = worldPos.xyz;

    vTexCoord = inTexCoord;
    vTexCoord.y = 1.0 - vTexCoord.y;

    mat3 normalMatrix = transpose(inverse(mat3(pc.model)));

    vec3 N = normalize(normalMatrix * inNormal);
    vec3 T = normalize(normalMatrix * inTangent.xyz);

    T = normalize(T - dot(T, N) * N);

    vec3 B = cross(N, T) * inTangent.w;

    vTBN = mat3(T, B, N);

    mat3 tbnInverse = transpose(vTBN);
    vec3 viewDir = cameraUBO.cameraPos - vWorldPos;
    vTangentViewDir = tbnInverse * viewDir;

    gl_Position = cameraUBO.viewProj * worldPos;
}