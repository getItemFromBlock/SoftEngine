#version 450

layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedo;

layout(location = 0) in vec2 vUV;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 worldPos = texture(gPosition, fragUV).rgb;
    vec3 normal   = texture(gNormal,   fragUV).rgb * 2.0 - 1.0; // unpack [0,1]→[-1,1]
    vec3 albedo   = texture(gAlbedo,   fragUV).rgb;

    outColor = vec4(albedo, 1.0);
}
