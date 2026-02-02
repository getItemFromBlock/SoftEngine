#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vTexCoord;
layout(location = 3) in vec3 vViewDir;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform Material
{
    vec4  color;
    float metallic;
    float roughness;
    float ao;
} material;

layout(binding = 2) uniform sampler2D albedoSampler;

layout(binding = 8) uniform Light
{
    vec3 position;
    vec3 color;
} light;

layout(binding = 9)  uniform samplerCube irradianceMap;
layout(binding = 10) uniform samplerCube prefilterMap;
layout(binding = 11) uniform sampler2D  brdfLUT;

const float PI = 3.14159265359;

float D_GGX(vec3 N, vec3 H, float r)
{
    float a = r * r;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float G_Schlick(float NdotV, float r)
{
    float k = (r + 1.0);
    k = (k * k) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float G_Smith(vec3 N, vec3 V, vec3 L, float r)
{
    return G_Schlick(max(dot(N, V), 0.0), r) *
    G_Schlick(max(dot(N, L), 0.0), r);
}

vec3 F_Schlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 F_SchlickRoughness(float cosTheta, vec3 F0, float r)
{
    return F0 + (max(vec3(1.0 - r), F0) - F0) *
    pow(1.0 - cosTheta, 5.0);
}

void main()
{
    vec3 albedo = pow(texture(albedoSampler, vTexCoord).rgb, vec3(2.2)) * material.color.rgb;

    float metallic  = material.metallic;
    float roughness = material.roughness;
    float ao        = material.ao;

    vec3 N = normalize(vNormal);
    vec3 V = normalize(vViewDir);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * albedo;

    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefiltered = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(brdfLUT, vec2(max(dot(N, V), 0.0), roughness)).rg;

    vec3 F = F_SchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);

    vec3 specularIBL = prefiltered * (F * brdf.x + brdf.y);
    vec3 ambient = (kD * diffuseIBL + specularIBL) * ao;

    vec3 color = ambient;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, material.color.a);
}