#version 450

layout(binding = 1) uniform sampler2D albedoSampler;

layout(location = 0) in vec2 vTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(vec3(1.0 - texture(albedoSampler, vTexCoord)), 1.0);
}