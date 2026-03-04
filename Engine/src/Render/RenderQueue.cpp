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
        cmd.AOTexture = material->GetTexture("aoSampler");
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
    if (!gBufferMaterial || !gBufferMaterial->GetShader()) return;

    auto* vulkanMaterial = gBufferMaterial->GetHandle();
    auto* pipeline = vulkanMaterial->GetPipeline();

    constexpr uint32_t TEXTURE_SET_INDEX = 0;
    VkDescriptorSetLayout setLayout = pipeline->GetDescriptorSetLayouts()[TEXTURE_SET_INDEX]->GetLayout();

    uint32_t maxFrames = renderer->GetMaxFramesInFlight();
    uint32_t frameIndex = renderer->GetFrameIndex();

    // --- Init descriptor pools ---
    if (!m_gBufferPoolInitialized)
    {
        m_gBufferPools.resize(maxFrames);
        std::vector<VkDescriptorPoolSize> sizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 512 * 2},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 * 5},
        };
        for (uint32_t i = 0; i < maxFrames; ++i)
        {
            m_gBufferPools[i] = std::make_unique<VulkanDescriptorPool>();
            m_gBufferPools[i]->Initialize(renderer->GetDevice(), sizes, 512, 0);
        }
        m_gBufferPoolInitialized = true;
    }

    // --- Init per-frame material data buffers ---
    if (!m_materialBuffersInitialized)
    {
        m_materialDataBuffers.resize(maxFrames);
        for (uint32_t i = 0; i < maxFrames; ++i)
        {
            m_materialDataBuffers[i].buffer = std::make_unique<VulkanUniformBuffer>();
            // 512 draws * sizeof(MaterialData)
            m_materialDataBuffers[i].buffer->Initialize(
                renderer->GetDevice(),
                sizeof(MaterialData) * 512,
                1, // single slot, we manage offsets manually
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
            m_materialDataBuffers[i].buffer->MapAll();
        }
        m_materialBuffersInitialized = true;
    }

    // Reset this frame's pool and material buffer write head
    m_gBufferPools[frameIndex]->Reset();
    m_materialDataBuffers[frameIndex].offset = 0;

    if (!renderer->BindShader(gBufferMaterial->GetShader().getPtr())) return;

    auto* currentScene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
    gBufferMaterial->SetAttribute("cameraUBO.viewProj", currentScene->GetCameraData().VP);
    gBufferMaterial->SendUBOValues(renderer);

    auto* camUBO = vulkanMaterial->GetUniformBuffer(0, 0);
    if (!camUBO)
    {
        PrintError("ExecuteGBuffer: missing camera UBO");
        return;
    }

    auto blank = Engine::Get()->GetResourceManager()->GetBlankTexture();
    if (!blank || !blank->SentToGPU())
    {
        PrintError("ExecuteGBuffer: blank texture not ready");
        return;
    }

    auto& matFrameBuffer = m_materialDataBuffers[frameIndex];
    VkBuffer matVkBuffer = matFrameBuffer.buffer->GetBuffer(0);

    Mesh* lastMesh = nullptr;

    for (auto& cmd : m_commands)
    {
        MaterialData matData{};
        matData.color = cmd.material->GetVec4Attribute("material.color");
        matData.roughnessFactor = cmd.material->GetFloatAttribute("material.roughnessFactor");
        matData.metalnessFactor = cmd.material->GetFloatAttribute("material.metalnessFactor");

        uint32_t matOffset = matFrameBuffer.offset;
        matFrameBuffer.buffer->UpdateDataAtOffset(&matData, sizeof(MaterialData), matOffset, 0);
        matFrameBuffer.offset += sizeof(MaterialData);

        VkDescriptorSet drawSet = m_gBufferPools[frameIndex]->Allocate(setLayout);
        if (drawSet == VK_NULL_HANDLE)
        {
            PrintError("ExecuteGBuffer: failed to allocate descriptor set");
            continue;
        }

        std::vector<VkWriteDescriptorSet> writes;
        std::vector<VkDescriptorBufferInfo> bufferInfos;
        std::vector<VkDescriptorImageInfo> imageInfos;
        writes.reserve(7);
        bufferInfos.reserve(2);
        imageInfos.reserve(5);

        auto PushUBO = [&](uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size)
        {
            bufferInfos.push_back({buffer, offset, size});
            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = drawSet;
            w.dstBinding = binding;
            w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w.descriptorCount = 1;
            w.pBufferInfo = &bufferInfos.back();
            writes.push_back(w);
        };

        // binding 0: camera UBO (shared, no offset)
        PushUBO(0, camUBO->GetBuffer(frameIndex), 0, camUBO->GetSize());

        // binding 1: this draw's material slice at its own offset
        PushUBO(1, matVkBuffer, matOffset, sizeof(MaterialData));

        auto PushTexture = [&](SafePtr<Texture>& tex, uint32_t binding)
        {
            Texture* t = (tex && tex->SentToGPU()) ? tex.getPtr() : blank.get();

            VkDescriptorImageInfo imgInfo{};
            imgInfo.sampler = t->GetBuffer()->GetSampler();
            imgInfo.imageView = t->GetBuffer()->GetImageView();
            imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos.push_back(imgInfo);

            VkWriteDescriptorSet w{};
            w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w.dstSet = drawSet;
            w.dstBinding = binding;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.descriptorCount = 1;
            w.pImageInfo = &imageInfos.back();
            writes.push_back(w);
        };

        PushTexture(cmd.albedoTexture, 2);
        PushTexture(cmd.normalTexture, 3);
        PushTexture(cmd.roughnessTexture, 4);
        PushTexture(cmd.metallicTexture, 5);
        PushTexture(cmd.AOTexture, 6);

        vkUpdateDescriptorSets(renderer->GetDevice()->GetDevice(),
                               static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);

        vkCmdBindDescriptorSets(renderer->GetCommandBuffer(),
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline->GetPipelineLayout(),
                                TEXTURE_SET_INDEX, 1, &drawSet,
                                0, nullptr);

        if (cmd.mesh != lastMesh)
        {
            renderer->BindVertexBuffers(cmd.mesh->GetVertexBuffer(),
                                        cmd.mesh->GetIndexBuffer());
            lastMesh = cmd.mesh;
        }

        PushConstant pushConstant = gBufferMaterial->GetShader()
                                                   ->GetPushConstants()[ShaderType::Vertex];
        renderer->SendPushConstants(&cmd.modelMatrix, sizeof(Mat4),
                                    gBufferMaterial->GetShader().getPtr(), pushConstant);

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
