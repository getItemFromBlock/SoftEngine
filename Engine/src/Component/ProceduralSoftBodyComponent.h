#pragma once
#include "IComponent.h"

#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"

#include "Resource/ComputeShader.h"

#include "Utils/Random.h"

#include <vector>
#include <unordered_map>
#include <list>

class Material;
class Mesh;

struct PBodySettings
{
    struct General
    {
        Vec2i particleAmount = Vec2i(11, 11);
        Vec2i surfacePoints = Vec2i(8, 8);
        float damping = 1.0f;
        float strength = 300.0f;
        uint32_t connectionStrength = 2;
    } general;

    struct SphereData
    {
        Vec3f position;
        float radius = 1.0f;
        bool animate = true;
    } sphereData;
};

struct PSBParticleData
{
    Vec3f position;
    uint32_t connectionsOffset;
    Vec3f velocity;
    uint32_t connectionsCount;
    Vec3f originalPos;
    uint32_t otherConnectionsCount;
};

struct PConnectionData0
{
    uint32_t particleID;
    float initialLength;
};

struct PConnectionData1
{
    uint32_t chunkID;
    float initialLength;
    Vec3f originalPos;
    uint32_t particleID;
};

struct GPUNeighborData
{
    Vec3f pos;
    int offset;
};

struct GPUChunkData
{
    Vec3f globalPos;
    int globalOffset;
    GPUNeighborData neighbors[8];
    Vec3f spherePos;
    float sphereRadius;
};

struct CPUChunkData
{
    Mesh *mesh;
    Vec3f localPosition;
    Vec2i iPos;
    uint32_t id;
    uint32_t globalOffset;
    uint32_t neighbors[8];
    uint32_t particleCount;
};

struct BufferChunk
{
    uint32_t id;
    uint32_t offset;
    uint32_t size;
    uint32_t occupied;
};

struct HeightPointData
{
    Vec3f pos;
    uint32_t id;
};

class ProceduralSoftBodyComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(ProceduralSoftBodyComponent);

    void Describe(ClassDescriptor& d) override;
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VulkanRenderer* renderer) override;
    void OnDestroy() override;

    PBodySettings& GetSettings() { return m_particleSettings; }

    SafePtr<Material> GetMaterial() const { return m_material; }
private:
    void CreateParticleBuffers();
    void CreateSkinnedMesh(CPUChunkData &chunkData);
    void MapMeshToParticles(CPUChunkData &data, std::vector<WeightedVertex> &vertices);
    void InitializeParticleData(std::vector<PSBParticleData> &particles, std::vector<PConnectionData0> &connections0,
                                std::vector<PConnectionData1> &connections1, const Vec2i &chunkID);
    void CreateHeightMap(const Vec2i &chunkID);

    Vec2i GetChunkPos(Vec3f pos);
    float GetHeightAt(float posX, float posZ);
    Vec3f GetNormalAt(const Vec3f &pos, float dt);
    uint32_t CreateChunkAt(Vec3f pos);
    void DeleteChunk(Vec2i iPos);

    BufferChunk AllocChunk(uint32_t size);
    void FreeChunk(uint32_t id);

private:
    std::unique_ptr<ComputeDispatch> m_simulationCompute0;
    std::unique_ptr<ComputeDispatch> m_simulationCompute1;

    std::unique_ptr<VulkanBuffer> m_particleBuffer;
    VkDeviceSize PBufSizeAligned;

    std::list<BufferChunk> memChunks;
    std::unordered_map<Vec2i, CPUChunkData> chunks;
    std::unordered_map<Vec2i, std::vector<HeightPointData>> heightData;
    uint32_t globalChunkCount;
    uint32_t globalChunkOffset;

    SafePtr<Mesh> m_billboardMesh;
    SafePtr<Material> m_material;
    SafePtr<Material> m_billboardMaterial;

    bool m_drawDebug = false;

    PBodySettings m_particleSettings;
};
