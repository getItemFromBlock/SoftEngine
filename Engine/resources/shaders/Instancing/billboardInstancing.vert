#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 4) in vec4 instancePosition;
layout(location = 5) in vec4 instanceColor;

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
    vec3 cameraRight;
    float _pad0;
    vec3 cameraUp;
    float _pad1;
    vec3 cameraFront;
    float _pad2;
} cameraUBO;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 vTexCoord;

void main()
{
    float scale = instancePosition.w;

    vec3 billboardOffset =
        cameraUBO.cameraRight * inPosition.x +
        cameraUBO.cameraUp    * inPosition.y +
        -cameraUBO.cameraFront * inPosition.z;

    vec3 worldPos = instancePosition.xyz + billboardOffset * scale;

    gl_Position = cameraUBO.viewProj * vec4(worldPos, 1.0);
    vTexCoord = inTexCoord;
    fragColor = instanceColor;
}
