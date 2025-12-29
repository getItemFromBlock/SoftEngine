#include "ParticleSystemComponent.h"
#include "Core/Engine.h"
#include "Render/Vulkan/VulkanIndexBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/VulkanVertexBuffer.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"
#include "Utils/Random.h"

void ParticleSystemComponent::Describe(ClassDescriptor& d)
{
    auto& countProp = d.AddInt("particleCount", m_particleCount);
    countProp.setter = [this](void* data) { SetParticleCount(*static_cast<int*>(data)); };

    auto& meshProp = d.AddProperty("Mesh", PropertyType::Mesh, &m_mesh);
    meshProp.setter = [this](void* data) { SetMesh(*static_cast<SafePtr<Mesh>*>(data)); };
}

void ParticleSystemComponent::OnCreate()
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();

    auto computeShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/ParticleCompute/particle.shader");
    auto instancingShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/Instancing/instancing.shader");

    m_material = resourceManager->CreateMaterial("ParticleInstancing");
    m_material->SetShader(instancingShader);

    m_mesh = resourceManager->Load<Mesh>(RESOURCE_PATH"/models/Cube.obj/Cube");
    SetParticleCount(50000);

    computeShader->EOnSentToGPU.Bind([this, computeShader, renderer]()
    {
        m_compute = computeShader->CreateDispatch(renderer);
        auto vkRenderer = Cast<VulkanRenderer>(renderer);
        auto device = vkRenderer->GetDevice();

        uint32_t count = static_cast<uint32_t>(m_particleCount);
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
            Vec3f p = Random::PointOnSphere(10.f);
            Vec3f d = Vec3f::Normalize(p);
            init[i].position = Vec4f(p.x, p.y, p.z, 1.f);
            init[i].velocity = Vec4f(d.x * 5.f, d.y * 5.f, d.z * 5.f, 0.f);
            init[i].color = Color::FromHSV((float(i) / count) * 360.f, 1.f, 1.f);
            init[i].padding = Vec4f(0.f);
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
}

void ParticleSystemComponent::OnUpdate(float deltaTime)
{
    if (!m_compute || !m_particleBuffer || !m_initialUploadComplete)
        return;
    
    if (m_needsRecreation)
    {
        RecreateParticleBuffers();
        m_needsRecreation = false;
        return;
    }

    auto particleBuffer = Cast<VulkanBuffer>(m_particleBuffer.get());
    auto instanceBuffer = Cast<VulkanBuffer>(m_instanceBuffer.get());
    auto renderer = Cast<VulkanRenderer>(Engine::Get()->GetRenderer());
    VkCommandBuffer cmd = renderer->GetCommandBuffer();

    VulkanMaterial* mat = Cast<VulkanMaterial>(m_compute->GetMaterial());

    uint32_t count = static_cast<uint32_t>(m_particleCount);
    
    mat->SetStorageBuffer(0, 0, particleBuffer->GetBuffer(), 0, 
        sizeof(ParticleData) * count, renderer);
    mat->SetStorageBuffer(0, 1, instanceBuffer->GetBuffer(), 0, 
        sizeof(InstanceData) * count, renderer);
    
    mat->BindForCompute(cmd, renderer->GetFrameIndex());

    struct Push { float dt; uint32_t c; } push;
    push.dt = deltaTime;
    push.c = count;

    vkCmdPushConstants(cmd, mat->GetPipeline()->GetPipelineLayout(),
        VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push), &push);

    uint32_t groups = (count + 63) / 64;
    mat->DispatchCompute(renderer, groups, 1, 1);

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
        m_instanceBuffer.get(), m_particleCount);
}

void ParticleSystemComponent::OnDestroy()
{
    Engine::Get()->GetRenderer()->WaitForGPU();

    if (m_particleBuffer) Cast<VulkanBuffer>(m_particleBuffer.get())->Cleanup();
    if (m_instanceBuffer) Cast<VulkanBuffer>(m_instanceBuffer.get())->Cleanup();
    if (m_stagingBuffer) Cast<VulkanBuffer>(m_stagingBuffer.get())->Cleanup();
}

void ParticleSystemComponent::SetParticleCount(int count)
{
    if (count <= 0) return;
    if (count == m_particleCount) return;

    m_particleCount = count;

    if (m_particleBuffer && m_initialUploadComplete) {
        m_needsRecreation = true;
    }
}

void ParticleSystemComponent::RecreateParticleBuffers()
{
    auto renderer = Cast<VulkanRenderer>(Engine::Get()->GetRenderer());
    auto device = renderer->GetDevice();

    renderer->WaitForGPU();

    if (m_particleBuffer)
        Cast<VulkanBuffer>(m_particleBuffer.get())->Cleanup();
    if (m_instanceBuffer)
        Cast<VulkanBuffer>(m_instanceBuffer.get())->Cleanup();
    if (m_stagingBuffer)
        Cast<VulkanBuffer>(m_stagingBuffer.get())->Cleanup();

    uint32_t count = static_cast<uint32_t>(m_particleCount);
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
        Vec3f p = Random::PointOnSphere(10.f);
        Vec3f d = Vec3f::Normalize(p);
        init[i].position = Vec4f(p.x, p.y, p.z, 1.f);
        init[i].velocity = Vec4f(d.x * 5.f, d.y * 5.f, d.z * 5.f, 0.f);
        init[i].color = Color::FromHSV((float(i) / count) * 360.f, 1.f, 1.f);
        init[i].padding = Vec4f(0.f);
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

void ParticleSystemComponent::SetMesh(SafePtr<Mesh> mesh)
{
    m_mesh = std::move(mesh);
}