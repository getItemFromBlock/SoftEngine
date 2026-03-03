#version 450
layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedo;
layout(set = 0, binding = 3) uniform sampler2D gMetallicRoughness;
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

// ── PBR helpers ───────────────────────────────────────────────────────────────

float distributionGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
    return geometrySchlickGGX(NdotV, roughness)
         * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0)
              * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main() {
    vec3  worldPos  = texture(gPosition, vUV).rgb;
    vec3  normal    = normalize(texture(gNormal, vUV).rgb * 2.0 - 1.0);
    vec3  albedo    = texture(gAlbedo, vUV).rgb;
    vec2  mr        = texture(gMetallicRoughness, vUV).rg;
    float metalness = mr.r;
    float roughness = clamp(mr.g, 0.04, 1.0);

    vec3  V     = normalize(lightData.cameraPos.xyz - worldPos);
    float NdotV = max(dot(normal, V), 0.0);

    // Dielectrics reflect ~4% at normal incidence; metals use their albedo colour
    vec3 F0 = mix(vec3(0.04), albedo, metalness);

    // ── Direct lighting ───────────────────────────────────────────────────────
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < lightData.numLights; i++) {
        Light light = lightData.lights[i];

        vec3  L_vec = light.position.xyz - worldPos;
        float dist  = length(L_vec);
        vec3  L     = normalize(L_vec);
        vec3  H     = normalize(V + L);

        float NdotL = max(dot(normal, L), 0.0);
        float NdotH = max(dot(normal, H), 0.0);
        float HdotV = max(dot(H,      V), 0.0);

        float attenuation = 1.0 / (dist * dist + 1.0);
        vec3  radiance    = light.color.rgb * light.color.a * attenuation;

        float D = distributionGGX(NdotH, roughness);
        float G = geometrySmith(NdotV, NdotL, roughness);
        vec3  F = fresnelSchlick(HdotV, F0);

        vec3  specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);
        vec3  kD       = (1.0 - F) * (1.0 - metalness);
        vec3  diffuse  = kD * albedo / PI;

        Lo += (diffuse + specular) * radiance * NdotL;
    }

    // ── Environment reflection (metals only) ──────────────────────────────────
    float maxMip = float(textureQueryLevels(envSampler) - 1);

    vec3 R              = reflect(-V, normal);
    float specMip       = roughness * maxMip;
    vec3 envColor       = textureLod(envSampler, R, specMip).rgb;
    vec3 envReflection  = envColor * F0 * metalness;

    vec3 ambient = vec3(0.03) * albedo; // small flat ambient to avoid pure black

    // ── Combine, tonemap, gamma correct ──────────────────────────────────────
    vec3 color = ambient + envReflection + Lo;
    color = color / (color + vec3(1.0)); // Reinhard
    color = pow(color, vec3(1.0 / 2.2)); // Gamma

    outColor = vec4(color, 1.0);
}

/*
layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedo;
layout(set = 0, binding = 3) uniform sampler2D gMetallicRoughness;
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

void main()
{
    // Sample GBuffer
    vec3 worldPos = texture(gPosition, vUV).rgb;
    vec3 N        = texture(gNormal,   vUV).rgb * 2.0 - 1.0; // decode
    N = normalize(N);

    // Reconstruct what the forward shader had as vViewDir/V
    vec3 V = normalize(lightData.cameraPos.xyz - worldPos);
    vec3 R = reflect(-V, N);

    vec3 color = texture(envSampler, R).rgb;
    outColor = vec4(color, 1.0);
}
*/