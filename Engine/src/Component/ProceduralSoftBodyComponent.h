#pragma once
#include "IComponent.h"

#include "Render/Vulkan/VulkanBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/VulkanMappedBuffer.h"

#include "Resource/ComputeShader.h"

#include "Utils/Random.h"

#include <vector>
#include <unordered_map>
#include <list>

class Material;
class Mesh;

namespace ProceduralSoftBody
{
    struct PBodySettings
    {
        struct General
        {
            Vec2i particleAmount = Vec2i(9, 9);
            Vec2i surfacePoints = Vec2i(8, 8);
            float damping = 1.0f;
            float strength = 300.0f;
            uint32_t connectionStrength = 1;
            float dtScale = 1.0f;
            bool paused = true;
        } general;

        struct SphereData
        {
            Vec3f position = Vec3f(0, 2.0f, 0);
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
        uint32_t connectionsLOffset;
        uint32_t connectionsLCount;
        uint32_t _paddingA, _paddingB, _paddingC;
    };

    struct PConnectionData0
    {
        uint32_t particleID;
        float initialLength;
    };

    struct PConnectionData1
    {
        Vec3f originalPos;
        uint32_t chunkID;
        uint32_t particleID;
        float initialLength;
        uint32_t _paddingA, _paddingB;
    };

    struct GPUNeighborData
    {
        Vec3f pos;
        int offset;
    };

    struct GPUChunkData
    {
        Vec3f		chunkPos;
        uint32_t	offset;
        uint32_t    particleCount;
        uint32_t    connectionOffset;
        uint32_t    connectionLOffset;
        uint32_t       _padding;
        GPUNeighborData neighbors[8];
    };

    struct GPUCommonData
    {
        Vec3f spherePos;
        float sphereRadius;
        Vec3f   gravity;
        float   deltaTime;
        float   damping;
        float   strength;
        uint32_t   _paddingA, _paddingB;
    };

    struct CPUChunkData
    {
        Mesh *mesh;
        Vec3f localPosition;
        Vec2i iPos;
        uint32_t pId;
        uint32_t cId;
        uint32_t lId;
        uint32_t globalOffsetP;
        uint32_t globalOffsetC;
        uint32_t globalOffsetL;
        uint32_t particleCount;
        uint32_t neighbors[8];
    };

    struct BufferChunk
    {
        uint32_t id;
        uint32_t offset;
        uint32_t size;
        uint32_t occupied;
    };

    struct CopyRequest
    {
        VkDeviceSize offsetP;
        VkDeviceSize sizeP;
        VkDeviceSize offsetC;
        VkDeviceSize sizeC;
        VkDeviceSize offsetL;
        VkDeviceSize sizeL;
    };

    struct Vec2iHash
    {
        size_t operator()(const Vec2i &k) const
        {
            return (size_t)(k.x) | ((size_t)(k.y) << 32);
        }
    };
    struct Vec3iHash
    {
        size_t operator()(const Vec3i &k) const
        {
            return ((size_t)(k.x) | ((size_t)(k.y) << 32)) ^
                (std::hash<int>()(k.z) << 1);
        }
    };

    struct PreChunkData
    {
        std::vector<uint32_t>   heightMap;
        std::vector<Vec3f>      positions;
        std::unordered_map<Vec3i, uint32_t, Vec3iHash> positionsMap;
    };
}

class ProceduralSoftBodyComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(ProceduralSoftBodyComponent);

    void Describe(ClassDescriptor &d) override;
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;
    void OnRender(VulkanRenderer *renderer) override;
    void OnDestroy() override;

    ProceduralSoftBody::PBodySettings &GetSettings() { return m_particleSettings; }

    SafePtr<Material> GetMaterial() const { return m_material; }
private:
    void CreateParticleBuffers();
    void CreateSkinnedMesh(ProceduralSoftBody::CPUChunkData &chunkData);
    void MapMeshToParticles(ProceduralSoftBody::CPUChunkData &data, std::vector<WeightedVertex> &vertices);
    void InitializeParticleData(std::vector<ProceduralSoftBody::PSBParticleData> &particles,
                                std::vector<ProceduralSoftBody::PConnectionData0> &connections0,
                                std::vector<ProceduralSoftBody::PConnectionData1> &connections1, const Vec2i &chunkID);
    void PreGenChunk(const Vec2i &chunkID);

    Vec2i GetChunkPos(Vec3f pos);
    float GetHeightAt(float posX, float posZ);
    Vec3f GetNormalAt(const Vec3f &pos, float dt);
    void CreateChunkAt(Vec2i pos);
    void DeleteChunk(Vec2i iPos);

    ProceduralSoftBody::BufferChunk AllocChunk(uint32_t size, uint32_t page);
    void FreeChunk(uint32_t id, uint32_t page);

private:
    std::unique_ptr<ComputeDispatch> m_simulationCompute0;
    std::unique_ptr<ComputeDispatch> m_simulationCompute1;

    VulkanMappedBuffer m_particleBuffer;
    VulkanMappedBuffer m_connectionBuffer;
    VulkanMappedBuffer m_connectionBufferL;
    VulkanMappedBuffer m_chunkDataBuffer;

    VkDeviceSize    m_atomicBufferAlignement;
    uint32_t        m_chunkBufferOffset;
    uint32_t        m_chunkSize;

    // Memory allocator stuff
    std::list<ProceduralSoftBody::BufferChunk> m_memChunks[3];
    uint32_t m_globalChunkCount[3];
    uint32_t m_globalChunkOffset[3];
    std::vector<ProceduralSoftBody::CopyRequest> copyRequests;

    std::unordered_map<Vec2i, ProceduralSoftBody::CPUChunkData, ProceduralSoftBody::Vec2iHash> m_chunks;
    std::unordered_map<Vec2i, ProceduralSoftBody::PreChunkData, ProceduralSoftBody::Vec2iHash> m_preChunkData;

    SafePtr<Mesh> m_billboardMesh;
    SafePtr<Mesh> m_sphereMesh;
    SafePtr<Material> m_material;
    std::vector<SafePtr<Material>> m_sphereMaterial;
    SafePtr<Material> m_billboardMaterial;

    bool m_drawDebug = false;

    ProceduralSoftBody::PBodySettings m_particleSettings;
};
