#version 450

layout(binding = 1) uniform Material {
    vec4 color; // fallback color
} material;

layout(binding = 2) uniform sampler2D albedoSampler;

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldNormal;
layout(location = 2) in vec2 vTexCoord;

layout(location = 0) out vec4 outPosition; // RGBA: xyz = view-space position, w=1
layout(location = 1) out vec4 outNormal;   // xyz = view-space normal (stored signed), w unused
layout(location = 2) out vec4 outAlbedo;   // rgb = albedo, a = 1 (or pack roughness/metal in a)

void main() {
    vec3 albedo = texture(albedoSampler, vTexCoord).rgb * material.color.rgb;

    outPosition = vec4(vWorldPos, 1.0);
    outNormal   = vec4(normalize(vWorldNormal) * 0.5 + 0.5, 0.0);
    outAlbedo   = vec4(albedo, 1.0);
}