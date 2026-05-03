#pragma once
#include <vector>
#define RENDER_QUEUE
#include <galaxymath/Maths.h>

#include "Resource/Material.h"
#include "Resource/Mesh.h"
#include "Resource/Shader.h"
#include "Utils/Type.h"

class VulkanRenderer;
class GameObject;
class Mesh;
class Material;
class Shader;
class Renderer;
class ProceduralSoftBodyComponent;

struct RenderCommand
{
    enum class Type
    {
        Mesh,
        SoftBody,
        SoftBodyDebug,
        SoftBodyChunk,
        SoftBodyChunkDebug
    };

    struct ChunkNeighborData
    {
        Vec3f pos;
        int offset;
    };

    struct ChunkRenderData
    {
        Vec3f   chunkPos;
        int     offset;
        ChunkNeighborData neighbors[8];
    };

    Type type = Type::Mesh;
    Mesh* mesh = nullptr;
    Material* material = nullptr;
    Shader* shader = nullptr;
    Mat4 modelMatrix = {};
    uint64_t sortKey = 0;

    // Mesh draw
    size_t subMeshIndex = 0;
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    SafePtr<Texture> albedoTexture, normalTexture,
                     roughnessTexture, metallicTexture,
                     AOTexture, heightTexture;

    // Soft body draw
    VkBuffer particleBuffer = VK_NULL_HANDLE;
    VkDeviceSize particleBufferSize = 0;
    uint32_t particleCount = 0;
    Vec3i particleGridSize = {};

    // Soft body chunk data
    ChunkRenderData chunkData;

    void GenerateSortKey();
    void GenerateSortKeyWithDepth(float depth);
};

class RenderQueue
{
public:
    enum class QueueType
    {
        Opaque,
        Transparent,
        UI
    };

    RenderQueue(QueueType type);
    ~RenderQueue();

    void Submit(const RenderCommand& command);

    void SubmitMeshRenderer(GameObject* gameObject, Mesh* mesh, const std::vector<SafePtr<Material>>& materials);
    void SubmitMeshRenderer(const Mat4 &modelMat, Mesh* mesh, const std::vector<SafePtr<Material>>& materials);
    void SubmitSoftBody(Mesh* mesh, Material* material, VkBuffer particleBuffer, VkDeviceSize particleBufferSize,
                        uint32_t particleCount, const Vec3i& gridSize,
                        const Mat4& transform, bool isDebug);
    void SubmitSoftBodyChunk(   Mesh* mesh, Material* material, VkBuffer particleBuffer, VkDeviceSize particleBufferSize,
                                const RenderCommand::ChunkRenderData &data, uint32_t instanceCount,
                                const Mat4& transform, bool isDebug);

    void Sort();

    void Execute(VulkanRenderer* renderer);
    void ExecuteGBuffer(VulkanRenderer* renderer, Material* gBufferMaterial);

    void Clear();

    size_t GetCommandCount() const { return m_commands.size(); }

private:
    std::vector<std::unique_ptr<VulkanDescriptorPool>> m_gBufferPools;
    bool m_gBufferPoolInitialized = false;

    struct PerFrameMaterialBuffer
    {
        std::unique_ptr<VulkanUniformBuffer> buffer;
        uint32_t offset = 0; // current write head in bytes
    };

    struct PerFrameChunkBuffer
    {
        std::unique_ptr<VulkanUniformBuffer> buffer;
        uint32_t offset = 0; // current write head in bytes
    };

    std::vector<PerFrameMaterialBuffer> m_materialDataBuffers;
    std::vector<PerFrameChunkBuffer> m_chunkDataBuffers;
    bool m_materialBuffersInitialized = false;
    uint32_t m_materialDataStride = 0;
    uint32_t m_chunkDataStride = 0;

    struct MaterialData
    {
        Vec4f color;
        float roughnessFactor;
        float metalnessFactor;
        float aoFactor;
        float heightScale;
    };

    QueueType m_type;

    std::vector<RenderCommand> m_commands;

    Texture* m_lastBoundAlbedo = nullptr;
    Texture* m_lastBoundNormal = nullptr;
    Texture* m_lastBoundRoughness = nullptr;
    Texture* m_lastBoundMetallic = nullptr;
    Texture* m_lastBoundAO = nullptr;
};

class RenderQueueManager
{
public:
    RenderQueueManager();
    void Cleanup();

    RenderQueue* GetOpaqueQueue() const { return m_opaqueQueue.get(); }
    RenderQueue* GetTransparentQueue() const { return m_transparentQueue.get(); }
    RenderQueue* GetUIQueue() const { return m_uiQueue.get(); }

    void SortAll() const;

    void ExecuteAll(VulkanRenderer* renderer) const;

    void ClearAll() const;

private:
    std::unique_ptr<RenderQueue> m_opaqueQueue;
    std::unique_ptr<RenderQueue> m_transparentQueue;
    std::unique_ptr<RenderQueue> m_uiQueue;
};
