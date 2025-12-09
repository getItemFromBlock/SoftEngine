#version 450

layout(binding = 1) uniform Material
{
    vec4 color;
} material;
        
layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
//   outColor = texture(albedoSampler, vTexCoord);
   outColor = material.color;
}