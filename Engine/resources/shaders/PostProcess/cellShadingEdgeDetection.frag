
#version 450

layout(binding = 1) uniform sampler2D albedoSampler;

layout(location = 0) in  vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

const int   BLUR_K          = 2;     // kernel size
const float BLUR_STDDEV     = 1.4;   // smoothing strength
const float LOW_THRESHOLD   = 0.08;  // weak-edge cutoff
const float HIGH_THRESHOLD  = 0.15;  // strong-edge cutoff
const vec3  EDGE_COLOR      = vec3(0.0);

ivec2 wrap(ivec2 c, ivec2 sz) {
    return ((c % sz) + sz) % sz;
}

float luma(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

// Gaussian-blurred luminance at an arbitrary texel coordinate.
float blurredLuma(ivec2 coord, ivec2 texSize) {
    int   width     = 2 * BLUR_K + 1;
    float total     = 0.0;
    float weightSum = 0.0;
    float twoSigSq  = 2.0 * BLUR_STDDEV * BLUR_STDDEV;

    for (int i = 1; i <= width; i++) {
        for (int j = 1; j <= width; j++) {
            ivec2 off    = ivec2(i - (BLUR_K + 1), j - (BLUR_K + 1));
            float mag    = float(off.x * off.x + off.y * off.y);
            float weight = exp(-mag / twoSigSq);
            total     += weight * luma(texelFetch(albedoSampler, wrap(coord + off, texSize), 0).rgb);
            weightSum += weight;
        }
    }
    return total / weightSum;
}

// Fetch the 3×3 blurred-luma neighbourhood centred on coord.
void lumaNeighbourhood(ivec2 coord, ivec2 texSize, out float n[3][3]) {
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            n[i][j] = blurredLuma(coord + ivec2(i - 1, j - 1), texSize);
}

// Sobel gradient magnitude + angle from a pre-fetched 3×3 neighbourhood.
void sobel(float n[3][3], out float magnitude, out float angleDeg) {
    float gx = -n[0][0] - 2.0*n[0][1] - n[0][2]
               +n[2][0] + 2.0*n[2][1] + n[2][2];

    float gy = -n[0][0] - 2.0*n[1][0] - n[2][0]
               +n[0][2] + 2.0*n[1][2] + n[2][2];

    magnitude = length(vec2(gx, gy));
    angleDeg  = degrees(atan(gy, gx));
    if (angleDeg < 0.0) 
        angleDeg += 180.0;
}

void main() {
    ivec2 texSize = textureSize(albedoSampler, 0);
    ivec2 coord   = ivec2(vTexCoord * vec2(texSize));

    float nb[3][3];
    lumaNeighbourhood(coord, texSize, nb);

    float gradMag, gradAngle;
    sobel(nb, gradMag, gradAngle);

    ivec2 d1, d2;
    if      (gradAngle <  22.5)               { d1 = ivec2( 1, 0); d2 = ivec2(-1,  0); }
    else if (gradAngle <  67.5)               { d1 = ivec2( 1, 1); d2 = ivec2(-1, -1); }
    else if (gradAngle < 112.5)               { d1 = ivec2( 0, 1); d2 = ivec2( 0, -1); }
    else if (gradAngle < 157.5)               { d1 = ivec2(-1, 1); d2 = ivec2( 1, -1); }
    else                                      { d1 = ivec2( 1, 0); d2 = ivec2(-1,  0); }

    float nb1[3][3]; lumaNeighbourhood(coord + d1, texSize, nb1);
    float nb2[3][3]; lumaNeighbourhood(coord + d2, texSize, nb2);

    float mag1, mag2, dummy;
    sobel(nb1, mag1, dummy);
    sobel(nb2, mag2, dummy);

    float edgeStrength = (gradMag >= mag1 && gradMag >= mag2) ? gradMag : 0.0;

    bool strongEdge = edgeStrength >= HIGH_THRESHOLD;
    bool weakEdge   = edgeStrength >= LOW_THRESHOLD;

    bool weakAccepted = false;
    if (weakEdge && !strongEdge) {
        for (int i = -1; i <= 1; i++) {
            for (int j = -1; j <= 1; j++) {
                if (i == 0 && j == 0) continue;
                float nbNb[3][3];
                lumaNeighbourhood(coord + ivec2(i, j), texSize, nbNb);
                float nm, nd;
                sobel(nbNb, nm, nd);
                float angle = degrees(atan(nd));
                if (nm >= HIGH_THRESHOLD) 
                {
                    weakAccepted = true;
                }
            }
        }
    }

    bool isEdge = strongEdge || weakAccepted;

    vec3 albedo = texelFetch(albedoSampler, coord, 0).rgb;
    outColor = vec4(mix(EDGE_COLOR, albedo, isEdge ? 0.0 : 1.0), 1.0);
}