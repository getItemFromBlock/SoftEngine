#version 450

layout(binding = 1) uniform sampler2D albedoSampler;

layout(binding = 0) uniform Params {
    float gamma;
} params;

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = texture(albedoSampler, vTexCoord).rgb;
    color = pow(color, vec3(1.0 / params.gamma));
    outColor = vec4(color, 1.0);
}