#version 450
layout(binding = 1) uniform sampler2D albedoSampler;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform Params {
    vec2 resolution;
    int pixelSize;
} params;

void main() {

    vec2 d = 1.0 / params.resolution;
    vec2 fragCoord = gl_FragCoord.xy;
    vec2 uv = (d.xy * float(params.pixelSize)) * floor(fragCoord.xy / float(params.pixelSize));

    vec4 color = texture(albedoSampler, uv);
    outColor = vec4(color.rgb, 1.0);

}
