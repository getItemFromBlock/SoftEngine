#version 450
layout(binding = 1) uniform sampler2D albedoSampler;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform Params {
    float radius;    // [0.0 - 1.0] how far from center the vignette starts. Default: 0.75
    float softness;  // [0.0 - 1.0] how gradual the falloff is.            Default: 0.45
    float intensity; // [0.0 - 1.0] strength of the darkening.             Default: 0.85
    vec3  color;     // RGB color of the vignette.                          Default: (0,0,0)
} params;

void main() {
    vec4 scene = texture(albedoSampler, vTexCoord);

    vec2 uv = vTexCoord - 0.5;
    float dist = length(uv);

    float vignette = smoothstep(params.radius, params.radius - params.softness, dist);
    vignette = 1.0 - vignette;

    vec3 vignetteColor = mix(scene.rgb, params.color, vignette * params.intensity);
    outColor = vec4(vignetteColor, scene.a);
}
