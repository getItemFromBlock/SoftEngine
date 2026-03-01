#include "RenderQueue.h"

#include <unordered_map>
#include <algorithm>

#include "Component/TransformComponent.h"
#include "Core/Engine.h"

#include "Resource/Mesh.h"

#include "Vulkan/VulkanRenderer.h"

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

RenderQueue::~RenderQueue()
{
    for (auto& mat : m_gBufferMaterialPool)
        mat->Unload();
    m_gBufferMaterialPool.clear();
    m_gBufferLastAlbedo.clear();
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
        if (subMeshes[i].count == 0 || materialCount == 0)
            continue;
        size_t materialIndex = i % materialCount;
        auto& material = materials[materialIndex];
     
        if (!material)
            continue;
        
        RenderCommand cmd;
        cmd.mesh = mesh;
        cmd.subMeshIndex = i;
        cmd.startIndex = subMeshes[i].startIndex;
        cmd.indexCount = subMeshes[i].count;
        cmd.material = material.getPtr();
        cmd.shader = material->GetShader().getPtr();
        cmd.modelMatrix = model;
        cmd.albedoTexture = material->GetTexture("albedoSampler");
        cmd.GenerateSortKey();
            
        Submit(cmd);
    }
}

void RenderQueue::SubmitInstancing(Mesh* mesh, Material* material, size_t instanceCount)
{
    RenderCommand cmd;
    cmd.mesh = mesh;
    
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

void RenderQueue::Execute(VulkanRenderer* renderer)
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

Material* RenderQueue::AcquireGBufferMaterial(size_t index, const Material* templateMaterial)
{
    if (index < m_gBufferMaterialPool.size())
        return m_gBufferMaterialPool[index].get();

    auto instance = std::make_unique<Material>("");
    instance->SetShader(templateMaterial->GetShader());

    m_gBufferMaterialPool.push_back(std::move(instance));
    return m_gBufferMaterialPool.back().get();
}

void RenderQueue::ExecuteGBuffer(VulkanRenderer* renderer, Material* gBufferMaterial)
{
    if (!gBufferMaterial || !gBufferMaterial->GetShader())
        return;

    if (!renderer->BindShader(gBufferMaterial->GetShader().getPtr()))
        return;

    const auto& cameraVP = Engine::Get()->GetSceneHolder()
                               ->GetCurrentScene()->GetCameraData().VP;

    for (size_t i = 0; i < m_commands.size(); ++i)
    {
        auto& cmd = m_commands[i];
        Material* mat = AcquireGBufferMaterial(i, gBufferMaterial);

        mat->SetAttribute("viewProj", cameraVP);
        mat->SetAttribute("color",    cmd.material->GetVec4Attribute("color"));

        Texture* albedo = cmd.albedoTexture.getPtr();
        if (i >= m_gBufferLastAlbedo.size())
            m_gBufferLastAlbedo.resize(i + 1, nullptr);

        if (albedo != m_gBufferLastAlbedo[i])
        {
            mat->SetAttribute("albedoSampler", cmd.albedoTexture);
            m_gBufferLastAlbedo[i] = albedo;
        }

        mat->SendAllValues(renderer);

        if (!renderer->BindMaterial(mat))
            continue;

        if (cmd.mesh)
            renderer->BindVertexBuffers(cmd.mesh->GetVertexBuffer(), cmd.mesh->GetIndexBuffer());

        PushConstant pushConstant = mat->GetShader()->GetPushConstants()[ShaderType::Vertex];
        renderer->SendPushConstants(&cmd.modelMatrix, sizeof(Mat4),
                                    mat->GetShader().getPtr(), pushConstant);

        renderer->DrawVertexSubMesh(cmd.mesh->GetIndexBuffer(), cmd.startIndex, cmd.indexCount);
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

void RenderQueueManager::Cleanup()
{
    m_opaqueQueue.reset();
    m_transparentQueue.reset();
    m_uiQueue.reset();
}

void RenderQueueManager::SortAll() const
{
    m_opaqueQueue->Sort();
    m_transparentQueue->Sort();
    m_uiQueue->Sort();
}

void RenderQueueManager::ExecuteAll(VulkanRenderer* renderer) const
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
