#include "GPUSoftBodyComponent.h"
#include "Core/Engine.h"

#include "Render/Vulkan/VulkanIndexBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/VulkanVertexBuffer.h"

#include "Resource/Mesh.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"
#include "Utils/Random.h"

// Aligns an integer to the next nearest memory aligned value. Alignement MUST be a power of two!
uint64_t align(uint64_t value, uint64_t alignement)
{
    assert((alignement-1) & alignement == 0);
    return (value + alignement - 1) & ~(alignement - 1);
}

void GPUSoftBodyComponent::Describe(ClassDescriptor& d)
{
    d.AddProperty("GPUSoftBodyComponent", PropertyType::ParticleSystem, this);
}

void GPUSoftBodyComponent::OnCreate()
{
    m_seed = Random::Global().Range(0, 100000);
    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();

    auto computeShader0 = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/softboy0.shader");
    auto computeShader1 = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/softboy1.shader");
    auto instancingShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/sf_instancing.shader");

    m_material = resourceManager->CreateMaterial("SoftbodyInstancing");
    m_material->SetShader(instancingShader);
    
    m_material->SetAttribute("albedoSampler", resourceManager->GetBlankTexture());

    m_mesh = resourceManager->Load<Mesh>(RESOURCE_PATH"/models/Cube.obj/Cube.mesh");

    computeShader0->EOnSentToGPU.Bind([this, computeShader0, renderer]()
        {
            m_simulationCompute0 = computeShader0->CreateDispatch(renderer);
        });
    
    CreateParticleBuffers();
    
    auto transform = p_gameObject->GetTransform();
    transform->EOnUpdateModelMatrix += [this]()
    {
        ApplySettings();  
    };
}

void GPUSoftBodyComponent::OnUpdate(float deltaTime)
{
    if (!m_simulationCompute0 || !m_simulationCompute1 || !m_particleBuffer || !m_initialUploadComplete)
        return;
    
    if (m_needsRecreation)
    {
        CreateParticleBuffers();
        m_needsRecreation = false;
        return;
    }

    auto renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer cmd = renderer->GetCommandBuffer();

    VulkanMaterial* mat0 = m_simulationCompute0->GetMaterial();
    VulkanMaterial* mat1 = m_simulationCompute1->GetMaterial();

    // First compute pass needs both particle data and connections
    mat0->SetStorageBuffer(0, 0, m_particleBuffer->GetBuffer(), 0,
                          PBufSizeAligned, renderer);
    mat0->SetStorageBuffer(0, 1, m_particleBuffer->GetBuffer(), PBufSizeAligned,
        CBufSizeAligned, renderer);

    mat0->BindForCompute(cmd, renderer->GetFrameIndex());

    struct Push
    {
        float deltaTime;
        uint32_t particleCount;
    } push;
    push.deltaTime = deltaTime;
    push.particleCount = totalParticleCount;
    

    vkCmdPushConstants(cmd, mat0->GetPipeline()->GetPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);

    uint32_t groups = (totalParticleCount + 63) / 64;
    mat0->DispatchCompute(renderer, groups, 1, 1);

    VkBufferMemoryBarrier barrier0{};
    barrier0.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier0.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier0.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier0.buffer = m_particleBuffer->GetBuffer();
    barrier0.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                         0, 0, nullptr, 1, &barrier0, 0, nullptr);


    // Second compute pass does not need connection data, as it just updates the velocity based on acceleration computed in the first pass
    mat1->SetStorageBuffer(0, 0, m_particleBuffer->GetBuffer(), 0,
        PBufSizeAligned, renderer);

    mat1->BindForCompute(cmd, renderer->GetFrameIndex());

    vkCmdPushConstants(cmd, mat1->GetPipeline()->GetPipelineLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);

    mat1->DispatchCompute(renderer, groups, 1, 1);

    VkBufferMemoryBarrier barrier1{};
    barrier1.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier1.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier1.buffer = m_particleBuffer->GetBuffer();
    barrier1.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0, 0, nullptr, 1, &barrier1, 0, nullptr);

    CameraData cam = p_gameObject->GetScene()->GetCameraData();
    m_material->SetAttribute("viewProj", cam.VP);
    m_material->SetAttribute("cameraRight", cam.right);
    m_material->SetAttribute("cameraUp", cam.up);
    m_material->SetAttribute("cameraFront", cam.forward);
}

void GPUSoftBodyComponent::OnRender(VulkanRenderer* renderer)
{
    if (!m_mesh || !m_mesh->IsLoaded() || !m_mesh->SentToGPU())
        return;
    
    if (!m_particleBuffer || !m_material || !m_initialUploadComplete)
        return;

    if (!renderer->BindShader(m_material->GetShader().getPtr()))
        return;

    /*
    // vertex shader needs particle data as source to "map" the mesh onto
    m_material->GetHandle()->SetStorageBuffer(0, 0, m_particleBuffer->GetBuffer(), 0,
        PBufSizeAligned, renderer);
    */

    m_material->SendAllValues(renderer);
    if (!renderer->BindMaterial(m_material.getPtr()))
        return;

    renderer->DrawInstanced(m_mesh->GetIndexBuffer(), m_mesh->GetVertexBuffer(),
                            m_particleBuffer.get(), totalParticleCount);
}

void GPUSoftBodyComponent::OnDestroy()
{
    Engine::Get()->GetRenderer()->WaitForGPU();

    if (m_particleBuffer) m_particleBuffer->Cleanup();
}

void GPUSoftBodyComponent::Restart()
{
    // TODO reset buffers ? hmleh
    /*
    Play();
    m_currentTime = 0.f;
    */
}

void GPUSoftBodyComponent::CreateParticleBuffers()
{
    auto renderer = Engine::Get()->GetRenderer();
    auto device = renderer->GetDevice();

    renderer->WaitForGPU();

    if (m_particleBuffer)
        m_particleBuffer->Cleanup();

    std::vector<ParticleData> particles;
    std::vector<ConnectionData> connections;
    InitializeParticleData(particles, connections);

    VkDeviceSize PBufSize = sizeof(ParticleData) * particles.size();
    VkDeviceSize CBufSize = sizeof(ConnectionData) * connections.size();
    PBufSizeAligned = align(PBufSize, 0x40);
    CBufSizeAligned = align(CBufSize, 0x40);

    auto particleBuffer = std::make_unique<VulkanBuffer>();
    particleBuffer->Initialize(device, PBufSizeAligned + CBufSizeAligned,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto stagingBuffer = std::make_unique<VulkanBuffer>();
    stagingBuffer->Initialize(device, PBufSizeAligned + CBufSizeAligned,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagingBuffer->CopyData(particles.data(), PBufSize);
    stagingBuffer->CopyData(particles.data(), CBufSize, PBufSizeAligned);

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandPool = renderer->GetCommandPool()->GetCommandPool();
    alloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device->GetDevice(), &alloc, &cmd);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);

    particleBuffer->CopyFrom(cmd, stagingBuffer.get(), PBufSizeAligned + CBufSizeAligned);

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.buffer = particleBuffer->GetBuffer();
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &barrier, 0, nullptr);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    vkQueueSubmit(device->GetGraphicsQueue().handle, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(device->GetGraphicsQueue().handle);

    vkFreeCommandBuffers(device->GetDevice(),
        renderer->GetCommandPool()->GetCommandPool(), 1, &cmd);

    m_initialUploadComplete = true;
    m_particleBuffer = std::move(particleBuffer);
    stagingBuffer->Cleanup();

    m_particleBuffer = std::move(particleBuffer);
}

void GPUSoftBodyComponent::InitializeParticleData(std::vector<ParticleData> &particles, std::vector<ConnectionData> &connections)
{
    const Vec3f worldPosition = p_gameObject->GetTransform()->GetWorldPosition();
    const Quat rotation = p_gameObject->GetTransform()->GetWorldRotation();
    const Vec3f scale = p_gameObject->GetTransform()->GetWorldScale();

    const Vec3i amount = m_particleSettings.general.particleAmount;
    const uint32_t maxL = m_particleSettings.general.connectionStrength;

    totalParticleCount = amount.x * amount.y * amount.z;
    particles.resize(totalParticleCount);

    for (int32_t j = 0; j < amount.y; j++)
    {
        for (int32_t i = 0; i < amount.x; i++)
        {
            for (int32_t k = 0; k < amount.z; k++)
            {
                const Vec3f offset = Vec3f(i, j, k) / (amount - Vec3i(1,1,1)) - Vec3f(0.5f, 0.5f, 0.5f);
                const Vec3f pos = worldPosition + offset * rotation * (scale * m_particleSettings.shape.radius * 2);
                const uint32_t index = i + j * (amount.x * amount.z) + k * (amount.x);

                particles[index].position = pos;
            }
        }
    }

    for (int32_t j = 0; j < amount.y; j++)
    {
        for (int32_t i = 0; i < amount.x; i++)
        {
            for (int32_t k = 0; k < amount.z; k++)
            {
                if (j < m_particleSettings.general.solidLayers)
                    continue;
                const uint32_t index0 = i + j * (amount.x * amount.z) + k * (amount.x);
                particles[index0].connectionsOffset = connections.size();

                for (int32_t l = i - maxL; l <= i + maxL; l++)
                {
                    if (l < 0 || l >= amount.x)
                        continue;
                    for (int32_t m = j - maxL; m <= j + maxL; m++)
                    {
                        if (m < 0 || m >= amount.y)
                            continue;
                        for (int32_t n = k - maxL; n <= k + maxL; n++)
                        {
                            if (n < 0 || n >= amount.z)
                                continue;
                            const uint32_t index1 = l + m * (amount.x * amount.z) + n * (amount.x);
                            if (index0 == index1)
                                continue;

                            ConnectionData c;
                            c.particleID = l + m * (amount.x * amount.z) + n * (amount.x);
                            c.initialLength = (particles[index0].position - particles[index1].position).Length();
                            connections.push_back(c);
                        }
                    }
                }
                particles[index0].connectionsCount = connections.size() - particles[index0].connectionsOffset;
            }
        }
    }
}

void GPUSoftBodyComponent::ApplySettings()
{
    m_needsRecreation = true;
}
