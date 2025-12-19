#include "RenderQueue.h"

#include <unordered_map>
#include <algorithm>

#include "Component/TransformComponent.h"
#include "Resource/Mesh.h"
#include "RHI/RHIRenderer.h"
#include "Scene/GameObject.h"

void RenderCommand::GenerateSortKey()
{
    uint64_t shaderKey = shader->GetUUID() >> 4;
    uint64_t materialKey = material->GetUUID() >> 4;
    uint64_t meshKey = mesh->GetUUID() >> 4;
        
    sortKey = ((shaderKey & 0xFFFF) << 48) |
        ((materialKey & 0xFFFF) << 32) |
        ((meshKey & 0xFFFF) << 16);
}

void RenderCommand::GenerateSortKeyWithDepth(float depth)
{
    GenerateSortKey();
    uint16_t depthKey = static_cast<uint16_t>(depth * 1000.0f);
    sortKey |= depthKey;
}

void RenderQueue::Submit(const RenderCommand& command)
{
    m_commands.push_back(command);
}

void RenderQueue::SubmitMeshRenderer(GameObject* gameObject, Mesh* mesh,
                                     const std::vector<SafePtr<Material>>& materials)
{
    if (!mesh || !mesh->IsLoaded() || !mesh->SentToGPU() || 
        !mesh->GetVertexBuffer() || !mesh->GetIndexBuffer())
        return;
        
    auto transformComponent = gameObject->GetComponent<TransformComponent>();
    auto model = transformComponent->GetWorldMatrix();
        
    size_t materialCount = materials.size();
    auto subMeshes = mesh->GetSubMeshes();
        
    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        size_t materialIndex = i % materialCount;
        auto& material = materials[materialIndex];
            
        RenderCommand cmd;
        cmd.mesh = mesh;
        cmd.subMeshIndex = i;
        cmd.startIndex = subMeshes[i].startIndex;
        cmd.indexCount = subMeshes[i].count;
        cmd.material = material.getPtr();
        cmd.shader = material->GetShader().getPtr();
        cmd.modelMatrix = model;
        cmd.GenerateSortKey();
            
        Submit(cmd);
    }
}

void RenderQueue::Sort()
{
    if (m_type == QueueType::Transparent)
    {
        std::sort(m_commands.begin(), m_commands.end(),
                  [](const RenderCommand& a, const RenderCommand& b) {
                      return a.sortKey > b.sortKey;
                  });
    }
    else
    {
        std::sort(m_commands.begin(), m_commands.end(),
                  [](const RenderCommand& a, const RenderCommand& b) {
                      return a.sortKey < b.sortKey;
                  });
    }
}

void RenderQueue::Execute(RHIRenderer* renderer)
{
    Material* lastMaterial = nullptr;
    Shader* lastShader = nullptr;
    Mesh* lastMesh = nullptr;
        
    for (auto& cmd : m_commands)
    {
        if (cmd.shader != lastShader)
        {
            if (!renderer->BindShader(cmd.shader))
                continue;
            lastShader = cmd.shader;
        }
            
        if (cmd.material != lastMaterial)
        {
            cmd.material->SendAllValues(renderer);
            if (!renderer->BindMaterial(cmd.material))
                continue;
            lastMaterial = cmd.material;
        }
            
        if (cmd.mesh != lastMesh)
        {
            renderer->BindVertexBuffers(cmd.mesh->GetVertexBuffer(), 
                                        cmd.mesh->GetIndexBuffer());
            lastMesh = cmd.mesh;
        }
            
        PushConstant pushConstant = cmd.shader->GetPushConstants()[ShaderType::Vertex];
        renderer->SendPushConstants(&cmd.modelMatrix, sizeof(Mat4), 
                                    cmd.shader, pushConstant);
        
        renderer->DrawVertexSubMesh(cmd.mesh->GetIndexBuffer(), 
                                    cmd.startIndex, 
                                    cmd.indexCount);
    }
}

std::vector<RenderBatch> RenderQueue::CreateBatches()
{
    std::vector<RenderBatch> batches;
        
    if (m_commands.empty())
        return batches;
        
    Sort();
        
    RenderBatch currentBatch;
    currentBatch.material = m_commands[0].material;
    currentBatch.shader = m_commands[0].shader;
    currentBatch.mesh = m_commands[0].mesh;
        
    for (const auto& cmd : m_commands)
    {
        if (cmd.material == currentBatch.material &&
            cmd.shader == currentBatch.shader &&
            cmd.mesh == currentBatch.mesh)
        {
            RenderBatch::Instance instance;
            instance.modelMatrix = cmd.modelMatrix;
            instance.startIndex = cmd.startIndex;
            instance.indexCount = cmd.indexCount;
            instance.subMeshIndex = cmd.subMeshIndex;
            currentBatch.instances.push_back(instance);
        }
        else
        {
            batches.push_back(currentBatch);
                
            currentBatch = RenderBatch();
            currentBatch.material = cmd.material;
            currentBatch.shader = cmd.shader;
            currentBatch.mesh = cmd.mesh;
                
            RenderBatch::Instance instance;
            instance.modelMatrix = cmd.modelMatrix;
            instance.startIndex = cmd.startIndex;
            instance.indexCount = cmd.indexCount;
            instance.subMeshIndex = cmd.subMeshIndex;
            currentBatch.instances.push_back(instance);
        }
    }
    
    if (!currentBatch.instances.empty())
        batches.push_back(currentBatch);
        
    return batches;
}

void RenderQueue::ExecuteBatched(RHIRenderer* renderer)
{
    auto batches = CreateBatches();
        
    for (auto& batch : batches)
    {
        if (!renderer->BindMaterial(batch.material))
            continue;
            
        renderer->BindVertexBuffers(batch.mesh->GetVertexBuffer(), 
                                    batch.mesh->GetIndexBuffer());
        
        for (auto& instance : batch.instances)
        {
            PushConstant pushConstant = batch.shader->GetPushConstants()[ShaderType::Vertex];
            renderer->SendPushConstants(&instance.modelMatrix, 
                                        sizeof(Mat4), 
                                        batch.shader, 
                                        pushConstant);
                
            renderer->DrawVertexSubMesh(batch.mesh->GetIndexBuffer(), 
                                        instance.startIndex, 
                                        instance.indexCount);
        }
    }
}

void RenderQueue::Clear()
{
    m_commands.clear();
}

RenderQueueManager::RenderQueueManager()
{
    m_opaqueQueue = std::make_unique<RenderQueue>(RenderQueue::QueueType::Opaque);
    m_transparentQueue = std::make_unique<RenderQueue>(RenderQueue::QueueType::Transparent);
    m_uiQueue = std::make_unique<RenderQueue>(RenderQueue::QueueType::UI);
}

void RenderQueueManager::SortAll() const
{
    m_opaqueQueue->Sort();
    m_transparentQueue->Sort();
    m_uiQueue->Sort();
}

void RenderQueueManager::ExecuteAll(RHIRenderer* renderer) const
{
    m_opaqueQueue->Execute(renderer);
    m_transparentQueue->Execute(renderer);
    m_uiQueue->Execute(renderer);
}

void RenderQueueManager::ClearAll() const
{
    m_opaqueQueue->Clear();
    m_transparentQueue->Clear();
    m_uiQueue->Clear();
}
