#version 450

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec3 camPos;
} cameraUBO;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vTexCoord;
layout(location = 3) out vec3 vViewDir;

void main()
{
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = cameraUBO.viewProj * worldPos;

    mat3 normalMatrix = transpose(inverse(mat3(pc.model)));
    vec3 N = normalize(normalMatrix * inNormal);

    vWorldPos = worldPos.xyz;
    vNormal   = N;
    vTexCoord = inTexCoord;
    vViewDir  = cameraUBO.camPos - worldPos.xyz;
}