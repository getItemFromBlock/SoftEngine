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

RenderQueue::RenderQueue(QueueType type) : m_type(type)
{
}

RenderQueue::~RenderQueue()
{
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
        cmd.normalTexture = material->GetTexture("normalSampler");
        cmd.roughnessTexture = material->GetTexture("roughnessSampler");
        cmd.metallicTexture = material->GetTexture("metalnessSampler");
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
        std::ranges::sort(m_commands,
                          [](const RenderCommand& a, const RenderCommand& b)
                          {
                              return a.sortKey > b.sortKey;
                          });
    }
    else
    {
        std::ranges::sort(m_commands,
                          [](const RenderCommand& a, const RenderCommand& b)
                          {
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
            auto currentScene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
            const auto& cameraVP = currentScene->GetCameraData().VP;
            
            cmd.material->SetAttribute("cameraUBO.viewProj", cameraVP, true);
            cmd.material->SetAttribute("cameraUBO.camPos", currentScene->GetCameraData().position, true);
            cmd.material->SetAttribute("debugCubemap", currentScene->GetEditorCamera()->GetSkybox(), true);
            
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

void RenderQueue::ExecuteGBuffer(VulkanRenderer* renderer, Material* gBufferMaterial)
{
    if (!gBufferMaterial || !gBufferMaterial->GetShader())
        return;

    if (!renderer->BindShader(gBufferMaterial->GetShader().getPtr()))
        return;

    auto currentScene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
    const auto& cameraVP = currentScene->GetCameraData().VP;
    gBufferMaterial->SetAttribute("cameraUBO.viewProj", cameraVP);

    m_lastBoundAlbedo = nullptr;
    m_lastBoundNormal = nullptr;
    Mesh* lastMesh = nullptr;

    for (auto& cmd : m_commands)
    {
        // Only rebind textures when they actually change
        bool texturesDirty = false;

        Texture* albedo = cmd.albedoTexture.getPtr();
        Texture* normal = cmd.normalTexture.getPtr();
        Texture* roughness = cmd.roughnessTexture.getPtr();
        Texture* metallic = cmd.metallicTexture.getPtr();

        if (albedo != m_lastBoundAlbedo)
        {
            gBufferMaterial->SetAttribute("albedoSampler", cmd.albedoTexture);
            m_lastBoundAlbedo = albedo;
            texturesDirty = true;
        }

        if (normal != m_lastBoundNormal)
        {
            gBufferMaterial->SetAttribute("normalSampler", cmd.normalTexture);
            m_lastBoundNormal = normal;
            texturesDirty = true;
        }

        if (roughness != m_lastBoundRoughness)
        {
            gBufferMaterial->SetAttribute("roughnessSampler", cmd.roughnessTexture);
            m_lastBoundRoughness = roughness;
            texturesDirty = true;
        }

        if (metallic != m_lastBoundMetallic)
        {
            gBufferMaterial->SetAttribute("metalnessSampler", cmd.metallicTexture);
            m_lastBoundMetallic = metallic;
            texturesDirty = true;
        }
        gBufferMaterial->SetAttribute("material.color", cmd.material->GetVec4Attribute("material.color"));
        gBufferMaterial->SetAttribute("material.roughnessFactor",
                                      cmd.material->GetFloatAttribute("material.roughnessFactor"));
        gBufferMaterial->SetAttribute("material.metalnessFactor",
                                      cmd.material->GetFloatAttribute("material.metalnessFactor"));


        gBufferMaterial->SendAllValues(renderer);
        if (!renderer->BindMaterial(gBufferMaterial))
            continue;

        if (cmd.mesh != lastMesh)
        {
            renderer->BindVertexBuffers(cmd.mesh->GetVertexBuffer(),
                                        cmd.mesh->GetIndexBuffer());
            lastMesh = cmd.mesh;
        }

        PushConstant pushConstant = gBufferMaterial->GetShader()
                                                   ->GetPushConstants()[ShaderType::Vertex];
        renderer->SendPushConstants(&cmd.modelMatrix, sizeof(Mat4),
                                    gBufferMaterial->GetShader().getPtr(),
                                    pushConstant);

        renderer->DrawVertexSubMesh(cmd.mesh->GetIndexBuffer(),
                                    cmd.startIndex, cmd.indexCount);
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
