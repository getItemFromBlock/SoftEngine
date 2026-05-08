#version 450

layout(set = 0, binding = 1) uniform Material {
    vec4  color;
    float roughnessFactor;
    float metalnessFactor;
    float aoFactor;
    float heightScale; // [0.02; 0.1] range
} material;

layout(set = 0, binding = 2) uniform sampler2D albedoSampler;
layout(set = 0, binding = 3) uniform sampler2D normalSampler;
layout(set = 0, binding = 4) uniform sampler2D roughnessSampler;
layout(set = 0, binding = 5) uniform sampler2D metalnessSampler;
layout(set = 0, binding = 6) uniform sampler2D aoSampler;
layout(set = 0, binding = 7) uniform sampler2D heightSampler;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in mat3 vTBN;
layout(location = 6) in vec3 vTangentViewDir;
layout(location = 7) in vec3 vColor;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outMetallicRoughnessAO;

void main()
{
    vec3 worldNormal   = normalize(vTBN * vec3(0,0,1));

    outPosition            = vec4(vWorldPos, 1.0);
    outNormal              = vec4(worldNormal, 0.0);
    outAlbedo              = vec4(vColor, 1.0);
    outMetallicRoughnessAO = vec4(0.0, 1.0, 1.0, 1.0);
}