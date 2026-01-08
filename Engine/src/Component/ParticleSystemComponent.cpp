#include "ParticleSystemComponent.h"
#include "Core/Engine.h"
#include "Render/Vulkan/VulkanIndexBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/VulkanVertexBuffer.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"
#include "Utils/Random.h"


template <typename T>
T MinMax<T>::RandomValue(Seed seed) const
{
    return min;
}

template <>
float MinMax<float>::RandomValue(Seed seed) const
{
    return Random::Range(min, max, seed);
}

template <>
Vec2f MinMax<Vec2f>::RandomValue(Seed seed) const
{
    return Random::Range(min, max, seed);
}

template <>
Vec3f MinMax<Vec3f>::RandomValue(Seed seed) const
{
    return Random::Range(min, max, seed);
}

template <>
Vec4f MinMax<Vec4f>::RandomValue(Seed seed) const
{
    return Random::Range(static_cast<Color>(min), static_cast<Color>(max), seed);
}

void ParticleSystemComponent::Describe(ClassDescriptor& d)
{
    auto& meshProp = d.AddProperty("Mesh", PropertyType::Mesh, &m_mesh);
    meshProp.setter = [this](void* data) { SetMesh(*static_cast<SafePtr<Mesh>*>(data)); };

    d.AddProperty("ParticleSystemComponent", PropertyType::ParticleSystem, this);
}

void ParticleSystemComponent::OnCreate()
{
    m_seed = Random::Global().Range(0, 100000);
    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();

    auto computeShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/ParticleCompute/particle.shader");
    auto instancingShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/Instancing/instancing.shader");

    m_material = resourceManager->CreateMaterial("ParticleInstancing");
    m_material->SetShader(instancingShader);
    
    m_material->SetAttribute("albedoSampler", resourceManager->GetBlankTexture());

    m_mesh = resourceManager->Load<Mesh>(RESOURCE_PATH"/models/Cube.obj/Cube");
    SetParticleCount(m_particleSettings.general.particleCount);

    computeShader->EOnSentToGPU.Bind([this, computeShader, renderer]()
    {
        m_compute = computeShader->CreateDispatch(renderer);
        auto vkRenderer = Cast<VulkanRenderer>(renderer);
        auto device = vkRenderer->GetDevice();

        uint32_t count = static_cast<uint32_t>(m_particleSettings.general.particleCount);
        VkDeviceSize particleBufferSize = sizeof(ParticleData) * count;
        VkDeviceSize instanceBufferSize = sizeof(InstanceData) * count;

        auto particleBuffer = std::make_unique<VulkanBuffer>();
        particleBuffer->Initialize(device, particleBufferSize,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        auto instanceBuffer = std::make_unique<VulkanBuffer>();
        instanceBuffer->Initialize(device, instanceBufferSize,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        auto stagingBuffer = std::make_unique<VulkanBuffer>();
        stagingBuffer->Initialize(device, particleBufferSize,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        std::vector<ParticleData> init(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            InitializeParticleData(init[i], i);
        }

        stagingBuffer->CopyData(init.data(), particleBufferSize);

        VkCommandBufferAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandPool = vkRenderer->GetCommandPool()->GetCommandPool();
        alloc.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(device->GetDevice(), &alloc, &cmd);

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);

        particleBuffer->CopyFrom(cmd, stagingBuffer.get(), particleBufferSize);

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
                             vkRenderer->GetCommandPool()->GetCommandPool(), 1, &cmd);

        m_initialUploadComplete = true;
        m_stagingBuffer = std::move(stagingBuffer);
        m_particleBuffer = std::move(particleBuffer);
        m_instanceBuffer = std::move(instanceBuffer);
    });
    
    auto transform = p_gameObject->GetTransform();
    transform->EOnUpdateModelMatrix += [this]()
    {
        ApplySettings();  
    };
}

void ParticleSystemComponent::OnUpdate(float deltaTime)
{
    if (!m_compute || !m_particleBuffer || !m_initialUploadComplete)
        return;

    if (m_needsShaderChange)
    {
        m_needsShaderChange = false;
        
        bool enable = m_particleSettings.rendering.billboard;
        auto resourceManager = Engine::Get()->GetResourceManager();
        auto renderer = Engine::Get()->GetRenderer();
    
        renderer->WaitForGPU();
    
        auto resourcePath = std::string(RESOURCE_PATH"/shaders/Instancing/") + 
                            ((enable) ? "billboardInstancing.shader" : "instancing.shader");
        auto instancingShader = resourceManager->Load<Shader>(resourcePath);
        m_material->SetShader(instancingShader);
        
        m_needsRecreation = true;
    }
    
    if (m_needsRecreation)
    {
        RecreateParticleBuffers();
        m_needsRecreation = false;
        return;
    }

    if (m_currentTime > m_particleSettings.general.duration &&
        m_particleSettings.general.looping)
    {
        Restart();
    }
    else if (m_currentTime < m_particleSettings.general.duration && m_isPlaying)
    {
        m_currentTime += deltaTime;
    }

    auto particleBuffer = Cast<VulkanBuffer>(m_particleBuffer.get());
    auto instanceBuffer = Cast<VulkanBuffer>(m_instanceBuffer.get());
    auto renderer = Cast<VulkanRenderer>(Engine::Get()->GetRenderer());
    VkCommandBuffer cmd = renderer->GetCommandBuffer();

    VulkanMaterial* mat = Cast<VulkanMaterial>(m_compute->GetMaterial());

    uint32_t count = static_cast<uint32_t>(m_particleSettings.general.particleCount);

    mat->SetStorageBuffer(0, 0, particleBuffer->GetBuffer(), 0,
                          sizeof(ParticleData) * count, renderer);
    mat->SetStorageBuffer(0, 1, instanceBuffer->GetBuffer(), 0,
                          sizeof(InstanceData) * count, renderer);

    mat->BindForCompute(cmd, renderer->GetFrameIndex());

    struct Push
    {
        float dt;
        float currentTime;
        uint32_t c;
    } push;
    push.dt = deltaTime;
    push.currentTime = m_currentTime;
    push.c = count;

    vkCmdPushConstants(cmd, mat->GetPipeline()->GetPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);

    uint32_t groups = (count + 63) / 64;
    mat->DispatchCompute(renderer, groups, 1, 1);

    if (m_debugReadbackEnabled && m_debugReadbackBuffer)
    {
        VkBufferMemoryBarrier computeToTransfer{};
        computeToTransfer.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        computeToTransfer.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        computeToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        computeToTransfer.buffer = particleBuffer->GetBuffer();
        computeToTransfer.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 1, &computeToTransfer, 0, nullptr
        );

        VkBufferCopy copy{};
        copy.size = sizeof(ParticleData) * count;
        vkCmdCopyBuffer(
            cmd,
            particleBuffer->GetBuffer(),
            m_debugReadbackBuffer->GetBuffer(),
            1, &copy
        );

        VkBufferMemoryBarrier transferToHost{};
        transferToHost.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        transferToHost.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        transferToHost.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        transferToHost.buffer = m_debugReadbackBuffer->GetBuffer();
        transferToHost.size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            0, 0, nullptr, 1, &transferToHost, 0, nullptr
        );
    }

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = instanceBuffer->GetBuffer();
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
                         0, 0, nullptr, 1, &barrier, 0, nullptr);

    CameraData cam = p_gameObject->GetScene()->GetCameraData();
    m_material->SetAttribute("viewProj", cam.VP);
    m_material->SetAttribute("cameraRight", cam.right);
    m_material->SetAttribute("cameraUp", cam.up);
    m_material->SetAttribute("cameraFront", cam.forward);

    if (m_debugReadbackEnabled)
    {
        ReadbackDebugData();
    }
}

void ParticleSystemComponent::OnRender(RHIRenderer* renderer)
{
    if (!m_mesh || !m_instanceBuffer || !m_material || !m_initialUploadComplete)
        return;

    if (!renderer->BindShader(m_material->GetShader().getPtr()))
        return;

    m_material->SendAllValues(renderer);
    if (!renderer->BindMaterial(m_material.getPtr()))
        return;

    renderer->DrawInstanced(m_mesh->GetIndexBuffer(), m_mesh->GetVertexBuffer(),
                            m_instanceBuffer.get(), m_particleSettings.general.particleCount);
}

void ParticleSystemComponent::OnDestroy()
{
    Engine::Get()->GetRenderer()->WaitForGPU();

    if (m_particleBuffer) m_particleBuffer->Cleanup();
    if (m_instanceBuffer) m_instanceBuffer->Cleanup();
    if (m_stagingBuffer) m_stagingBuffer->Cleanup();
    if (m_debugReadbackBuffer) m_debugReadbackBuffer->Cleanup();
}

void ParticleSystemComponent::SetParticleCount(int count)
{
    if (count <= 0) 
        return;
    if (count == m_particleSettings.general.particleCount) 
        return;

    m_particleSettings.general.particleCount = count;

    if (m_particleBuffer && m_initialUploadComplete)
    {
        m_needsRecreation = true;
    }
}

void ParticleSystemComponent::Play()
{
    m_isPlaying = true;
}

void ParticleSystemComponent::Pause()
{
    m_isPlaying = false;
}

void ParticleSystemComponent::Restart()
{
    Play();
    m_currentTime = 0.f;
}

void ParticleSystemComponent::SetBillboard(bool enable)
{
    m_particleSettings.rendering.billboard = enable;
    m_needsShaderChange = true;
}

void ParticleSystemComponent::RecreateParticleBuffers()
{
    auto renderer = Cast<VulkanRenderer>(Engine::Get()->GetRenderer());
    auto device = renderer->GetDevice();

    renderer->WaitForGPU();

    if (m_particleBuffer)
        m_particleBuffer->Cleanup();
    if (m_instanceBuffer)
        m_instanceBuffer->Cleanup();
    if (m_stagingBuffer)
        m_stagingBuffer->Cleanup();
    if (m_debugReadbackBuffer)
        m_debugReadbackBuffer->Cleanup();

    uint32_t count = static_cast<uint32_t>(m_particleSettings.general.particleCount);
    VkDeviceSize particleBufferSize = sizeof(ParticleData) * count;
    VkDeviceSize instanceBufferSize = sizeof(InstanceData) * count;

    auto particleBuffer = std::make_unique<VulkanBuffer>();
    particleBuffer->Initialize(device, particleBufferSize,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                               VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto instanceBuffer = std::make_unique<VulkanBuffer>();
    instanceBuffer->Initialize(device, instanceBufferSize,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    auto stagingBuffer = std::make_unique<VulkanBuffer>();
    stagingBuffer->Initialize(device, particleBufferSize,
                              VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (m_debugReadbackEnabled)
    {
        m_debugReadbackBuffer = std::make_unique<VulkanBuffer>();
        m_debugReadbackBuffer->Initialize(
            device,
            particleBufferSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        m_debugCPUBuffer.resize(count);
    }


    std::vector<ParticleData> init(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        InitializeParticleData(init[i], i);
    }

    stagingBuffer->CopyData(init.data(), particleBufferSize);

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

    particleBuffer->CopyFrom(cmd, stagingBuffer.get(), particleBufferSize);

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

    m_particleBuffer = std::move(particleBuffer);
    m_instanceBuffer = std::move(instanceBuffer);
    m_stagingBuffer = std::move(stagingBuffer);
}

void ParticleSystemComponent::InitializeParticleData(ParticleData& p, uint32_t index) const
{
    Vec3f worldPosition = p_gameObject->GetTransform()->GetWorldPosition();
    Vec3f forward = p_gameObject->GetTransform()->GetForward();
    
    Seed seed = m_seed + index;
    Vec3f point;
    Vec3f direction;

    using ShapeType = ParticleSettings::Shape::Type;
    switch (m_particleSettings.shape.type) {
    case ShapeType::None:
        point = worldPosition;
        direction = forward;
        break;
    case ShapeType::Sphere:
        point = worldPosition + Random::PointInSphere(m_particleSettings.shape.radius, seed);
        direction = (point - worldPosition).GetNormalize();
        break;
    case ShapeType::Cube:
        break;
    case ShapeType::Cone:
        break;
    }
    
    float size = m_particleSettings.general.startSize.GetValue(seed);
    float lifeTime = m_particleSettings.general.startLifeTime.GetValue(seed);
    float rate = m_particleSettings.emission.rateOverTime.GetValue(seed);
    bool prewarm = m_particleSettings.general.preWarm;
    float preWarmTime = m_particleSettings.general.duration;
    float startTime = index / rate;
    float startSpeed = m_particleSettings.general.startSpeed.GetValue(seed);
    
    if (prewarm)
        startTime -= preWarmTime;

    p.positionSize = Vec4f(point, size);
    p.velocityLifeTime = Vec4f(direction * startSpeed, lifeTime);
    p.color = m_particleSettings.general.startColor.GetValue(seed);
    float gravity = m_particleSettings.general.gravityScale.GetValue(seed);
    p.startTimeGravity = Vec4f(startTime, gravity, 0.f, 0.f);
}

void ParticleSystemComponent::ReadbackDebugData()
{
    if (!m_debugReadbackEnabled || !m_debugReadbackBuffer)
        return;

    auto renderer = Cast<VulkanRenderer>(Engine::Get()->GetRenderer());

    renderer->WaitForGPU();

    void* mapped = nullptr;
    vkMapMemory(
        renderer->GetDevice()->GetDevice(),
        m_debugReadbackBuffer->GetBufferMemory(),
        0,
        VK_WHOLE_SIZE,
        0,
        &mapped
    );

    memcpy(m_debugCPUBuffer.data(), mapped,
           sizeof(ParticleData) * m_debugCPUBuffer.size());

    vkUnmapMemory(
        renderer->GetDevice()->GetDevice(),
        m_debugReadbackBuffer->GetBufferMemory()
    );

    ParticleData first = m_debugCPUBuffer[0];
    first;
}

void ParticleSystemComponent::SetMesh(SafePtr<Mesh> mesh)
{
    m_mesh = std::move(mesh);
}

void ParticleSystemComponent::ApplySettings()
{
    m_needsRecreation = true;
}
