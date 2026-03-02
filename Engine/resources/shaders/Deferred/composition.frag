#version 450

layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedo;

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

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 worldPos = texture(gPosition, vUV).rgb;
    vec3 normal   = normalize(texture(gNormal, vUV).rgb * 2.0 - 1.0);
    vec3 albedo   = texture(gAlbedo, vUV).rgb;

    vec3 V = normalize(lightData.cameraPos.xyz - worldPos);
    vec3 accumulatedLighting = vec3(0.0);

    for (int i = 0; i < lightData.numLights; i++)
    {
        Light light = lightData.lights[i];

        vec3  L_vec = light.position.xyz - worldPos;
        float dist  = length(L_vec);
        vec3  L     = normalize(L_vec);
        vec3  H     = normalize(L + V);

        float NdotL = max(dot(normal, L), 0.0);
        float NdotH = max(dot(normal, H), 0.0);

        float attenuation = 1.0 / (dist * dist + 1.0);
        vec3  radiance    = light.color.rgb * light.color.a * attenuation;

        float specPower        = 64.0;
        float specNormalization = (specPower + 8.0) / (8.0 * 3.14159);
        float specTerm         = pow(NdotH, specPower) * specNormalization;

        vec3 diffuse  = albedo * (1.0 / 3.14159);
        vec3 specular = vec3(0.04) * specTerm;

        accumulatedLighting += (diffuse + specular) * radiance * NdotL;
    }

    vec3 color = 0.03 * albedo + accumulatedLighting;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}