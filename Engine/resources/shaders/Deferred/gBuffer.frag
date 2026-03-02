#version 450

layout(set = 0, binding = 1) uniform Material {
    vec4 color;
} material;

layout(set = 0, binding = 2) uniform sampler2D albedoSampler;
layout(set = 0, binding = 3) uniform sampler2D normalSampler;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in mat3 vTBN;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedo;

void main() {
    vec3 albedo = texture(albedoSampler, vTexCoord).rgb * material.color.rgb;

    vec3 tangentNormal = texture(normalSampler, vTexCoord).rgb * 2.0 - 1.0;

    vec3 worldNormal = normalize(vTBN * tangentNormal);

    outPosition = vec4(vWorldPos, 1.0);
    outNormal   = vec4(worldNormal * 0.5 + 0.5, 0.0);
    outAlbedo   = vec4(albedo, 1.0);
}