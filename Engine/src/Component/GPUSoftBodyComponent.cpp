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
    assert(((alignement-1) & alignement) == 0);
    return (value + alignement - 1) & ~(alignement - 1);
}

void GPUSoftBodyComponent::Describe(ClassDescriptor& d)
{
    auto &res = d.AddVec3i("Block Size", m_particleSettings.general.particleAmount).SetRangeInt(3, 1024);
    res.onModified = [this](void)
        {
            m_needsRecreation = true;
        };

    auto &res2 = d.AddProperty("Connections Amount", PropertyType::Int, &m_particleSettings.general.connectionStrength)
        .SetRangeInt(1, 256);
    res2.onModified = [this](void)
        {
            m_needsRecreation = true;
        };

    auto &res3 = d.AddInt("Solid Layers", m_particleSettings.general.solidLayers).SetRangeInt(1, 1024);
    res3.onModified = [this](void)
        {
            m_needsRecreation = true;
        };

    d.AddFloat("Damping", m_particleSettings.general.damping).SetRangeFloat(0, 65536);

    d.AddFloat("Connection Strength", m_particleSettings.general.strength).SetRangeFloat(0, 65536);

    auto &res6 = d.AddEnum("Shape", reinterpret_cast<int32_t *>(&m_particleSettings.shape.type), m_particleSettings.shape.to_cstr());
    res6.onModified = [this](void)
        {
            m_needsRecreation = true;
        };

    d.AddButton("Reset")
        .onModified = [this](void)
        {
            m_needsRecreation = true;
        };
}

void GPUSoftBodyComponent::OnCreate()
{
    m_seed = Random::Global().Range(0, 100000);
    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();

    auto computeShader0 = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/softbody0.shader");
    auto computeShader1 = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/softbody1.shader");
    auto instancingShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/sb_instancing.shader");

    m_material = resourceManager->CreateMaterial("SoftbodyInstancing");
    m_material->SetShader(instancingShader);
    
    m_material->SetAttribute("albedoSampler", resourceManager->GetBlankTexture());

    m_mesh = resourceManager->Load<Mesh>(RESOURCE_PATH"/models/Cube.obj/Cube.mesh");

    computeShader0->EOnSentToGPU.Bind([this, computeShader0, renderer]()
        {
            m_simulationCompute0 = computeShader0->CreateDispatch(renderer);
        });

    computeShader1->EOnSentToGPU.Bind([this, computeShader1, renderer]()
        {
            m_simulationCompute1 = computeShader1->CreateDispatch(renderer);
        });
    
    CreateParticleBuffers();
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
    //VulkanMaterial* mat2 = m_material->GetHandle();

    // First compute pass needs both particle data and connections
    mat0->SetStorageBuffer(0, 0, m_particleBuffer->GetBuffer(), 0,
                            PBufSizeAligned, renderer);
    mat0->SetStorageBuffer(0, 1, m_particleBuffer->GetBuffer(), PBufSizeAligned,
                            CBufSizeAligned, renderer);

    mat0->BindForCompute(cmd, renderer->GetFrameIndex());

    struct Push0
    {
        Vec3f gravity;
        float deltaTime;
        float damping;
        float strength;
        uint32_t  particleCount;
    } push0;

    push0.gravity = GetGameObject()->GetTransform()->GetWorldRotation().GetInverse() * Vec3f(0, 9.81f, 0);
    push0.deltaTime = std::min(deltaTime, 1/60.0f);
    push0.damping = m_particleSettings.general.damping;
    push0.strength = m_particleSettings.general.strength;
    push0.particleCount = totalParticleCount;

    vkCmdPushConstants(cmd, mat0->GetPipeline()->GetPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push0), &push0);

    uint32_t groups = (totalParticleCount + 63) / 64;
    mat0->DispatchCompute(renderer, groups, 1, 1);

    VkBufferMemoryBarrier barrier0{};
    barrier0.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier0.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier0.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier0.buffer = m_particleBuffer->GetBuffer();
    barrier0.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                         0, 0, nullptr, 1, &barrier0, 0, nullptr);


    // Second compute pass does not need connection data, as it just updates the position based on the velocity computed in the first pass
    mat1->SetStorageBuffer(0, 0, m_particleBuffer->GetBuffer(), 0,
                            PBufSizeAligned, renderer);

    mat1->BindForCompute(cmd, renderer->GetFrameIndex());

    struct Push1
    {
        float deltaTime;
        uint32_t particleCount;
    } push1;

    push1.deltaTime = deltaTime;
    push1.particleCount = totalParticleCount;

    vkCmdPushConstants(cmd, mat1->GetPipeline()->GetPipelineLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push1), &push1);

    mat1->DispatchCompute(renderer, groups, 1, 1);

    VkBufferMemoryBarrier barrier1{};
    barrier1.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
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

    
    // vertex shader needs particle data as source to "map" the mesh onto
    m_material->GetHandle()->SetStorageBuffer(0, 2, m_particleBuffer->GetBuffer(), 0,
        PBufSizeAligned, renderer);
    
    struct Push
    {
        Mat4 transform;
        Vec3i size;
    } push;

    push.transform = GetGameObject()->GetTransform()->GetWorldMatrix().GetTranspose();
    push.size = m_particleSettings.general.particleAmount;


    vkCmdPushConstants(renderer->GetCommandBuffer(), m_material->GetHandle()->GetPipeline()->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push), &push);

    m_material->SendAllValues(renderer);
    if (!renderer->BindMaterial(m_material.getPtr()))
        return;

    renderer->DrawInstanced(m_mesh->GetIndexBuffer(), m_mesh->GetVertexBuffer(), totalParticleCount);
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

    std::vector<SBParticleData> particles;
    std::vector<ConnectionData> connections;
    InitializeParticleData(particles, connections);

    VkDeviceSize PBufSize = sizeof(SBParticleData) * particles.size();
    VkDeviceSize CBufSize = sizeof(ConnectionData) * connections.size();
    PBufSizeAligned = align(PBufSize, 0x40);
    PBufSizeAligned = std::max(0x40llu, PBufSize);
    CBufSizeAligned = align(CBufSize, 0x40);
    CBufSizeAligned = std::max(0x40llu, CBufSize);

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
    if (CBufSize)
        stagingBuffer->CopyData(connections.data(), CBufSize, PBufSizeAligned);

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
}

void GPUSoftBodyComponent::InitializeParticleData(std::vector<SBParticleData> &particles, std::vector<ConnectionData> &connections)
{
    const Vec3i amount = m_particleSettings.general.particleAmount;
    const int32_t maxL = m_particleSettings.general.connectionStrength;

    totalParticleCount = amount.x * amount.y * amount.z;
    particles.resize(totalParticleCount);

    for (int32_t j = 0; j < amount.y; j++)
    {
        for (int32_t i = 0; i < amount.x; i++)
        {
            for (int32_t k = 0; k < amount.z; k++)
            {
                const Vec3f offset = Vec3f(i / float(amount.x-1), j / float(amount.y-1), k / float(amount.z-1)) - Vec3f(0.5f, 0.5f, 0.5f);
                const Vec3f pos = offset * (m_particleSettings.shape.scale * 2);
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
                particles[index0].connectionsOffset = (uint32_t)connections.size();

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
                particles[index0].connectionsCount = (uint32_t)connections.size() - particles[index0].connectionsOffset;
            }
        }
    }

    if (m_particleSettings.shape.type != BodySettings::Shape::Type::Cube)
    {
        std::vector<uint32_t> toRemove;
        const Vec3f center = Vec3f(0.5f, 0.5f, 0.5f);
        const float maxDist = powf((m_particleSettings.shape.scale), 2) + 0.01f;
        SBParticleData *pPointer = &particles[0];

        for (uint32_t i = 0; i < totalParticleCount; i++)
        {
            Vec3f delta = particles[i].position;
            switch (m_particleSettings.shape.type)
            {
            case BodySettings::Shape::Type::Sphere:
                if (delta.LengthSquared() > maxDist)
                    toRemove.push_back(i);
                break;
            case BodySettings::Shape::Type::Cone:
                if (Vec2f(delta.x, delta.z).Length() > (-delta.y / 2 + 0.5f + 0.01f))
                    toRemove.push_back(i);
                break;
            default:
                break;
            }
        }
        toRemove.push_back((uint32_t)particles.size());

        SBParticleData *ptr = pPointer + toRemove[0];
        for (uint32_t i = 0; i < toRemove.size() - 1; i++)
        {
            std::copy(pPointer + toRemove[i] + 1, pPointer + toRemove[i + 1], ptr);
            ptr += ((pPointer + toRemove[i + 1]) - (pPointer + toRemove[i] + 1));
        }

        for (uint32_t i = 1; i < toRemove.size(); i++)
        {
            particles.pop_back();
        }

        for (uint32_t i = 0; i < particles.size(); i++)
        {
            ConnectionData *cOffset = &connections[particles[i].connectionsOffset];
            for (uint32_t j = 0; j < particles[i].connectionsCount; j++)
            {
                uint32_t offset = 0;
                for (uint32_t k = 0; k < toRemove.size()-1; k++)
                {
                    if (cOffset[j].particleID == toRemove[k]) // Particle was deleted
                    {
                        std::copy(cOffset + (j + 1), cOffset + particles[i].connectionsCount, cOffset + j);
                        particles[i].connectionsCount--;
                        j--;
                        offset = 0;
                        break;
                    }
                    else if (cOffset[j].particleID < toRemove[k])
                        break;
                    offset++;
                }
                if (offset)
                    cOffset[j].particleID -= offset;
                assert(j >= particles[i].connectionsCount || cOffset[j].particleID != i);
            }
        }
    }
    totalParticleCount = uint32_t(particles.size());
}

void GPUSoftBodyComponent::ApplySettings()
{
    m_needsRecreation = true;
}
