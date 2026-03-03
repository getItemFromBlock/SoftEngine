#version 450
layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedo;
layout(set = 0, binding = 3) uniform sampler2D gMetallicRoughness; // r=metalness, g=roughness, b=ao
layout(set = 0, binding = 4) uniform samplerCube envSampler;

struct Light {
    vec4 position;
    vec4 color;
};

layout(std430, set = 1, binding = 0) readonly buffer LightBuffer {
    int   numLights;
    float _pad0;
    float _pad1;
    float _pad2;
    vec4  cameraPos;
    Light lights[32];
} lightData;

layout(location = 0) in  vec2 vUV;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

void main()
{
    // ───── GBuffer Sampling ─────
    vec3 worldPos = texture(gPosition, vUV).rgb;

    vec3 N = texture(gNormal, vUV).rgb * 2.0 - 1.0;
    N = normalize(N);

    vec3 albedo = texture(gAlbedo, vUV).rgb;

    vec3 mra = texture(gMetallicRoughness, vUV).rgb;
    float metalness = mra.r;
    float roughness = mra.g;
    float ao        = mra.b;

    // ───── View vectors ─────
    vec3 V = normalize(lightData.cameraPos.xyz - worldPos);
    vec3 R = reflect(-V, N);

    // ───── Fresnel base reflectance (F0) ─────
    vec3 F0 = vec3(0.04);             // default dielectric
    F0 = mix(F0, albedo, metalness);  // metals use albedo as F0

    // ───── Simple Fresnel (Schlick) ─────
    float NdotV = max(dot(N, V), 0.0);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);

    // ───── Specular IBL ─────
    vec3 specular = texture(envSampler, R).rgb * F;

    // ───── Diffuse term (only for non-metals) ─────
    vec3 diffuse = (1.0 - metalness) * albedo / PI;

    // For now we use env map as diffuse ambient approximation
    vec3 ambient = texture(envSampler, N).rgb;

    vec3 color = (diffuse * ambient + specular) * ao;

    outColor = vec4(color, 1.0);
}