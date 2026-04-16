#version 450

layout(binding = 1) uniform sampler2D albedoSampler;

layout(location = 0) in vec2 vTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(albedoSampler, vTexCoord);
    float Y = 0.299 * outColor.r + 0.587 * outColor.g + 0.114 * outColor.b;
    outColor = vec4(Y, Y, Y, 1.0);
}