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

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;
layout(location = 3) out vec4 outMetallicRoughnessAO;

const float POM_MIN_LAYERS = 8.0;
const float POM_MAX_LAYERS = 32.0;

float sampleHeight(vec2 uv)
{
    float h = texture(heightSampler, uv).r;
    return mix(1.0 - h, h, 1.0);
}

vec2 parallaxOcclusionMapping(vec2 uv, vec3 viewDirTS)
{
    float numLayers = mix(POM_MAX_LAYERS, POM_MIN_LAYERS,
        abs(dot(vec3(0.0, 0.0, 1.0), viewDirTS)));

    float layerDepth   = 1.0 / numLayers;
    float currentDepth = 0.0;

    vec2 deltaUV = (viewDirTS.xy / viewDirTS.z) * material.heightScale / numLayers;
    deltaUV.y *= -1.0;

    vec2  currentUV     = uv;
    float currentHeight = sampleHeight(currentUV);

    while (currentDepth < currentHeight) {
        currentUV     -= deltaUV;
        currentHeight  = sampleHeight(currentUV);
        currentDepth  += layerDepth;
    }

    vec2  prevUV      = currentUV + deltaUV;
    float afterDepth  = currentHeight - currentDepth;
    float beforeDepth = sampleHeight(prevUV) - (currentDepth - layerDepth);

    float weight = afterDepth / (afterDepth - beforeDepth);
    return mix(currentUV, prevUV, weight);
}

void main() {
    vec3 viewDirTS = normalize(vTangentViewDir);
    vec2 uv = parallaxOcclusionMapping(vTexCoord, viewDirTS);
    vec3 albedo = texture(albedoSampler, uv).rgb * material.color.rgb;

    vec3 tangentNormal = normalize(texture(normalSampler,     uv).rgb * 2.0 - 1.0);
    vec3 worldNormal   = normalize(vTBN * tangentNormal);
    float roughness    = texture(roughnessSampler, uv).g * material.roughnessFactor;
    float metalness    = texture(metalnessSampler, uv).b * material.metalnessFactor;
    float bakedAO      = texture(aoSampler,        uv).r * material.aoFactor;

    outPosition            = vec4(vWorldPos, 1.0);
    outNormal              = vec4(worldNormal, 0.0);
    outAlbedo              = vec4(albedo, 1.0);
    outMetallicRoughnessAO = vec4(metalness, roughness, bakedAO, 1.0);
}