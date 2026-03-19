#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in ivec4 inIndices;
layout(location = 5) in vec4 inWeights;

struct ParticleData {
    vec3 position;
	uint connectionsOffset;
    vec3 velocity;
	uint connectionsCount;
	vec3 originalPos;
	float unused;
};

layout(push_constant) uniform Push {
	mat4 transform;
    ivec3 size;
} pc;

layout(set = 0, binding = 2) readonly buffer Particles {
    ParticleData particles[];
};

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
} cameraUBO;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 vTexCoord;

void main() {
	vec3 pos = inPosition;
	for (int i = 0; i < 4; i++)
	{
		if (inIndices[i] < 0)
			break;
		int id = inIndices[i];
		pos += (particles[id].position - particles[id].originalPos) * inWeights[i];
	}

    vec4 worldPos = vec4(pos, 1.0);
	worldPos = worldPos * pc.transform;
    gl_Position = cameraUBO.viewProj * worldPos;
    vTexCoord = inTexCoord;
	
    fragColor = vec4(inTangent.xyz, 1);
}
