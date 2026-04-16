#version 450
layout(binding = 1) uniform sampler2D albedoSampler;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform Params {
    vec2 resolution;
    float radius;    // [1 - 4] kernel radius in pixels.           Default: 2.0
    float sigma;     // [0.5 - 4.0] Gaussian standard deviation.  Default: 1.5
} params;

float gaussian(float x, float sigma) {
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

void main() {
    vec4  colorSum   = vec4(0.0);
    float weightSum  = 0.0;
    int   iRadius    = int(ceil(params.radius));

    for (int x = -iRadius; x <= iRadius; x++) {
        for (int y = -iRadius; y <= iRadius; y++) {
            float wx = gaussian(float(x), params.sigma);
            float wy = gaussian(float(y), params.sigma);
            float w  = wx * wy;

            vec2 offset = vec2(float(x), float(y)) * (1.0 / params.resolution);
            colorSum  += texture(albedoSampler, vTexCoord + offset) * w;
            weightSum += w;
        }
    }

    outColor = colorSum / weightSum;
}
