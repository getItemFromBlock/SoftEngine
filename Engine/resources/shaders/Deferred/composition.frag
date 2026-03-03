#version 450
layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedo;
layout(set = 0, binding = 3) uniform sampler2D gMetallicRoughness;
layout(set = 0, binding = 4) uniform samplerCube envSampler;
layout(set = 0, binding = 5) uniform samplerCube irradianceSampler;

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

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness)
    * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
    * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main()
{
    // ───── GBuffer ─────
    vec3  worldPos  = texture(gPosition,          vUV).rgb;
    vec3  N         = normalize(texture(gNormal,  vUV).rgb * 2.0 - 1.0);
    vec3  albedo    = texture(gAlbedo,             vUV).rgb;
    vec3  mra       = texture(gMetallicRoughness, vUV).rgb;

    float metalness = mra.r;
    float roughness = max(mra.g, 0.05); // clamp to avoid perfect mirror artifacts
    float ao        = mra.b;

    vec3  V     = normalize(lightData.cameraPos.xyz - worldPos);
    vec3  R     = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    // ───── Direct Lighting ─────
    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lightData.numLights; ++i)
    {
        vec3  lightPos    = lightData.lights[i].position.xyz;
        vec3  lightColor  = lightData.lights[i].color.rgb;
        float lightPower  = lightData.lights[i].color.a;

        vec3  L           = normalize(lightPos - worldPos);
        vec3  H           = normalize(V + L);
        float dist        = length(lightPos - worldPos);
        float attenuation = 1.0 / (dist * dist);
        vec3  radiance    = lightColor * lightPower * attenuation;
        float NdotL       = max(dot(N, L), 0.0);

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  spec  = (NDF * G * F) / (4.0 * NdotV * NdotL + 0.0001);
        vec3  kD    = (vec3(1.0) - F) * (1.0 - metalness);

        Lo += (kD * albedo / PI + spec) * radiance * NdotL;
    }

    // ───── IBL Diffuse ─────
    vec3 F_ibl   = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD_ibl  = (vec3(1.0) - F_ibl) * (1.0 - metalness);
    vec3 diffuseIBL = kD_ibl * texture(irradianceSampler, N).rgb * albedo;

    // ───── IBL Specular ─────
    // Without a BRDF LUT, use this energy-conserving approximation
    float maxLOD = 4.0;
    vec3 prefilteredColor = textureLod(envSampler, R, roughness * maxLOD).rgb;

    // Split-sum approximation without LUT (Karis 2013 simplified)
    vec3 envBRDF  = mix(vec3(0.04), vec3(1.0), pow(1.0 - NdotV, 5.0) * (1.0 - roughness));
    vec3 specularIBL = prefilteredColor * envBRDF * F_ibl;

    // ───── Combine ─────
    // ao only on ambient, not direct light
    vec3 ambient = (diffuseIBL + specularIBL) * ao;
    vec3 color   = ambient + Lo;

    // ───── Exposure + Tonemap + Gamma ─────
    float exposure = 1.0; // lower this (e.g. 0.5) if still too bright
    color = vec3(1.0) - exp(-color * exposure); // exposure tone mapping
    color = pow(color, vec3(1.0 / 2.2));        // gamma correction

    outColor = vec4(color, 1.0);
}