#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in ivec4 inIndices;
layout(location = 5) in vec4 inWeights;
layout(location = 6) in ivec4 inChunks;

struct ParticleData {
    vec3 position;
	uint connectionsOffset;
    vec3 velocity;
	uint connectionsCount;
	vec3 originalPos;
	uint connectionsLOffset;
	uint connectionsLCount;
	uint _paddingA, _paddingB, _paddingC;
};

struct NeighborData
{
	vec3 pos;
	int offset;
};

layout(set = 0, binding = 9) uniform ChunkRenderData {
	vec3 chunkPos;
	int offset;
	NeighborData neighbors[8];
} chunkData;

layout(set = 0, binding = 8) readonly buffer Particles {
    ParticleData particles[];
};

layout(std140, set = 0, binding = 0) uniform CameraUBO {
    mat4 viewProj;
	vec3 cameraPos;
} cameraUBO;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) out mat3 vTBN;
layout(location = 6) out vec3 vTangentViewDir;

void main() {
	vec3 pos = inPosition;
	for (int i = 0; i < 4; i++)
	{
		if (inIndices[i] < 0)
			break;
		
		int id = inIndices[i];
		int chunk = inChunks[i];
		if (chunk < 0)
		{
			pos += (particles[id + chunkData.offset].position - particles[id + chunkData.offset].originalPos) * inWeights[i];
		}
		else if (chunkData.neighbors[id].offset >= 0)
		{
			pos += (particles[id + chunkData.neighbors[id].offset].position - particles[id + chunkData.neighbors[id].offset].originalPos) * inWeights[i];
		}
	}

    vec4 worldPos = vec4(pos + chunkData.chunkPos, 1.0);
	
	gl_Position = cameraUBO.viewProj * worldPos;
    vWorldPos = worldPos.xyz;
	
    vTexCoord = inTexCoord;
	vTexCoord.y = 1.0 - vTexCoord.y;

    vec3 N = normalize(inNormal);
    vec3 T = normalize(inTangent.xyz);

    T = normalize(T - dot(T, N) * N);

    vec3 B = cross(N, T) * inTangent.w;

    vTBN = mat3(T, B, N);

    mat3 tbnInverse = transpose(vTBN);
    vec3 viewDir = cameraUBO.cameraPos - vWorldPos;
    vTangentViewDir = tbnInverse * viewDir;
}
