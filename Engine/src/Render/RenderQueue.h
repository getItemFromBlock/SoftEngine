#pragma once
#include <vector>
#define RENDER_QUEUE
#include <galaxymath/Maths.h>

#include "Resource/Material.h"
#include "Resource/Mesh.h"
#include "Resource/Shader.h"
#include "Utils/Type.h"

class RHIRenderer;
class GameObject;
class Mesh;
class Material;
class Shader;
class Renderer;

struct RenderCommand
{
    Mesh* mesh;
    size_t subMeshIndex;
    uint32_t startIndex;
    uint32_t indexCount;
    
    Material* material;
    Shader* shader;
    
    Mat4 modelMatrix;
    
    uint64_t sortKey;
    
    void GenerateSortKey();

    void GenerateSortKeyWithDepth(float depth);
};

struct RenderBatch
{
    Material* material;
    Shader* shader;
    Mesh* mesh;
    
    struct Instance
    {
        Mat4 modelMatrix;
        uint32_t startIndex;
        uint32_t indexCount;
        size_t subMeshIndex;
    };
    
    std::vector<Instance> instances;
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
    
    RenderQueue(QueueType type) : m_type(type) {}
    
    void Submit(const RenderCommand& command);

    void SubmitMeshRenderer(GameObject* gameObject, Mesh* mesh, const std::vector<SafePtr<Material>>& materials);

    void Sort();

    void Execute(RHIRenderer* renderer);

    std::vector<RenderBatch> CreateBatches();

    void ExecuteBatched(RHIRenderer* renderer);

    void Clear();

    size_t GetCommandCount() const { return m_commands.size(); }
    
private:
    QueueType m_type;
    std::vector<RenderCommand> m_commands;
};

class RenderQueueManager
{
public:
    RenderQueueManager();

    RenderQueue* GetOpaqueQueue() const { return m_opaqueQueue.get(); }
    RenderQueue* GetTransparentQueue() const { return m_transparentQueue.get(); }
    RenderQueue* GetUIQueue() const { return m_uiQueue.get(); }
    
    void SortAll() const;

    void ExecuteAll(RHIRenderer* renderer) const;

    void ClearAll() const;

private:
    std::unique_ptr<RenderQueue> m_opaqueQueue;
    std::unique_ptr<RenderQueue> m_transparentQueue;
    std::unique_ptr<RenderQueue> m_uiQueue;
};