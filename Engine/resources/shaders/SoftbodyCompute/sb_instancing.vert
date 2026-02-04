#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

struct ParticleData {
    vec3 position;
	uint connectionsOffset;
    vec3 velocity;
	uint connectionsCount;
};

layout(push_constant) uniform Push {
    ivec3 size;
    float unused;
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
    vec4 worldPos = vec4(inPosition * 0.025 + particles[gl_InstanceIndex].position, 1.0);
    gl_Position = cameraUBO.viewProj * worldPos;
    vTexCoord = inTexCoord;
	int a = gl_InstanceIndex / (pc.size.x * pc.size.z);
	int b = gl_InstanceIndex % (pc.size.x * pc.size.z);
	
	int c = b / pc.size.x;
	b -= c * pc.size.x;
	
    fragColor = vec4(vec3(a,b,c)/vec3(pc.size), 1);
}
