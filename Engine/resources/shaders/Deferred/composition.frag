#version 450 core

layout(location = 0) in vec2 TexCoords;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedo;
layout(set = 0, binding = 3) uniform sampler2D gMetallicRoughnessAO;
layout(set = 0, binding = 5) uniform samplerCube irradianceSampler;
layout(set = 0, binding = 6) uniform sampler2D   brdfLut;
layout(set = 0, binding = 7) uniform samplerCube prefilteredSampler;

struct Light {
    vec4 position;  // xyz = world pos OR direction, w = dir or pos
    vec4 direction; // xyz = light dir OR deltaPos, w = dir or dtpos
    vec4 angles;    // xy = spot light angles, z = attenuation
    vec4 color;     // xyz = linear RGB, w = intensity
};

layout(std430, set = 1, binding = 0) readonly buffer LightBuffer {
    int   numLights;
    float _pad0;
    float _pad1;
    float _pad2;
    vec4  cameraPos;
    Light lights[32];
} lightData;

const float PI                 = 3.14159265359;
const float MAX_REFLECTION_LOD = 4.0;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a * a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom  = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / denom;
}

float GeometrySchlickGGX_Direct(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith_Direct(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX_Direct(NdotV, roughness) *
           GeometrySchlickGGX_Direct(NdotL, roughness);
}

float GeometrySchlickGGX_IBL(float NdotV, float roughness)
{
    float k = (roughness * roughness) / 2.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith_IBL(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX_IBL(NdotV, roughness) *
           GeometrySchlickGGX_IBL(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) *
           pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 ACESFilmic(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3  fragPos   = texture(gPosition,            TexCoords).rgb;
    vec3  N         = normalize(texture(gNormal,    TexCoords).rgb);
    vec4  albedoA   = texture(gAlbedo,              TexCoords);
    vec3  albedo    = albedoA.rgb;
    float alpha     = albedoA.a;
    vec3  mra       = texture(gMetallicRoughnessAO, TexCoords).rgb;
    float metallic  = mra.r;
    float roughness = mra.g;
    float ao        = mra.b;

    vec3 V = normalize(lightData.cameraPos.xyz - fragPos);
    vec3 R = reflect(-V, N);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    for (int i = 0; i < lightData.numLights; ++i)
    {
        vec3  AB          = lightData.lights[i].position.xyz - fragPos * lightData.lights[i].position.w;
        
        vec3  pa          = -AB;
        vec3  ba          = lightData.lights[i].direction.xyz;
        float bas         = max(dot(ba,ba), 0.001);
        float height      = clamp( dot(pa,ba)/bas, 0.0, 1.0 );
        vec3  distV       = pa - ba*height*lightData.lights[i].direction.w;
        float lensqr      = dot(distV, distV);
        
        vec3  L           = normalize(-distV);
        float fact        = dot(L, N);
        if (fact < 0.5 && lightData.lights[i].direction.w > 0.)
        {
            vec3 L1 = normalize(AB);
            vec3 L2 = normalize(AB + ba);
            if (dot(L2, N) > dot(L1, N))
                L1 = L2;
            if (dot(L1, N) > fact)
                L = mix(L1, L, smoothstep(0.0, 0.5, fact));
        }
        vec3  H           = normalize(V + L);
        float attenP      = 1.0 / lensqr;
        float factor      = lensqr * lightData.lights[i].angles.z;
        float sFactor     = clamp(1.0 - factor * factor, 0, 1);
        float SdotL       = dot(lightData.lights[i].direction.xyz, L);
        float attenS      = clamp(SdotL * lightData.lights[i].angles.x + lightData.lights[i].angles.y, 0, 1);
        
        vec3  radiance    = lightData.lights[i].color.rgb *
                            (lightData.lights[i].color.w  *
                            attenP * sFactor * sFactor * attenS * attenS);

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith_Direct(N, V, L, roughness);
        vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3  numerator   = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3  specular    = numerator / denominator;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }
    
    // IBL ambient
    vec3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 irradiance = texture(irradianceSampler, N).rgb;
    vec3 diffuse    = irradiance * albedo;

    vec3 prefilteredColor = textureLod(prefilteredSampler, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf             = texture(brdfLut, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular         = prefilteredColor * (F * brdf.x + brdf.y);

    vec3 ambient = (kD * diffuse + specular) * ao;

    vec3 color = ambient + Lo;

    color = ACESFilmic(color);
    // gamma correct
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, alpha);
}
