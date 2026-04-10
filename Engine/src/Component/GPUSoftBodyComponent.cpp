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
    ASSERT(((alignement-1) & alignement) == 0);
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
        .SetRangeInt(2, 256);
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

    d.AddBool("Debug", m_drawDebug);

    d.AddButton("Reset")
        .onModified = [this](void)
        {
            m_needsRecreation = true;
        };
}

void GPUSoftBodyComponent::CreateFromMesh(SafePtr<Mesh> inputMesh)
{
    m_initializerMesh = inputMesh;
    m_loadedFromMesh = true;
    InitializeParticleDataFromMesh(5, 0.5);
    CreateParticleBuffers();
}

void GPUSoftBodyComponent::OnCreate()
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();

    auto computeShader0 = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/softbody0.shader");
    auto computeShader1 = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/softbody1.shader");
    auto instancingShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/sb_instancing.shader");
    auto skinnedShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/SoftbodyCompute/sb_skinning.shader");

    m_billboardMaterial = resourceManager->CreateMaterial("SoftbodyInstancing");
    m_billboardMaterial->SetShader(instancingShader);
    m_billboardMaterial->SetAttribute("albedoSampler", resourceManager->GetBlankTexture());
    
    m_material = resourceManager->CreateMaterial("SoftbodySkinned", skinnedShader);
    m_material->SetAttribute("albedoSampler", resourceManager->GetBlankTexture());
    m_material->SetAttribute("normalSampler", resourceManager->GetDefaultNormal());
    m_material->SetAttribute("roughnessSampler", resourceManager->GetBlankTexture());
    m_material->SetAttribute("metalnessSampler", resourceManager->GetBlankTexture());
    m_material->SetAttribute("aoSampler", resourceManager->GetBlankTexture());
    m_material->SetAttribute("heightSampler", resourceManager->GetBlackTexture());

    m_material->SetAttribute("material.color", Vec4f::One());
    m_material->SetAttribute("material.roughnessFactor", 1.f);
    m_material->SetAttribute("material.metalnessFactor", 1.f);
    m_material->SetAttribute("material.aoFactor", 1.f);
    m_material->SetAttribute("material.heightScale", 0.0f);

    m_mesh = std::make_shared<Mesh>("internal");
    m_billboardMesh = resourceManager->Load<Mesh>(RESOURCE_PATH"/models/Cube.obj/Cube.mesh");

    computeShader0->EOnSentToGPU.Bind([this, computeShader0, renderer]()
        {
            m_simulationCompute0 = computeShader0->CreateDispatch(renderer);
        });

    computeShader1->EOnSentToGPU.Bind([this, computeShader1, renderer]()
        {
            m_simulationCompute1 = computeShader1->CreateDispatch(renderer);
        });
}

void GPUSoftBodyComponent::OnUpdate(float deltaTime)
{
    if (!m_simulationCompute0 || !m_simulationCompute1)
        return;
    
    if (m_needsRecreation)
    {
        if (m_loadedFromMesh)
        {
            InitializeParticleDataFromMesh(5, 0.5);
        }
        CreateParticleBuffers();
        m_needsRecreation = false;
        return;
    }

    if (!m_particleBuffer)
        return;

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
    push0.particleCount = m_totalParticleCount;

    vkCmdPushConstants(cmd, mat0->GetPipeline()->GetPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Push0), &push0);

    uint32_t groups = (m_totalParticleCount + 63) / 64;
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

    push1.deltaTime = std::min(deltaTime, 1 / 60.0f);
    push1.particleCount = m_totalParticleCount;

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
    m_material->SetAttribute("cameraUBO.viewProj", cam.VP);
    if (m_drawDebug)
    {
        m_billboardMaterial->SetAttribute("cameraUBO.viewProj", cam.VP);
    }
}

void GPUSoftBodyComponent::OnRender(VulkanRenderer* renderer)
{
    if (!m_mesh || !m_mesh->IsLoaded() || !m_mesh->HasBeenSent()) 
        return;
    if (!m_particleBuffer || !m_material) 
        return;

    auto* rqm = Engine::Get()->GetRenderer()->GetRenderQueueManager();
    auto* queue = rqm->GetOpaqueQueue();

    const Mat4 transform = GetGameObject()->GetTransform()
                               ->GetWorldMatrix().GetTranspose();

    // Skinned soft body mesh
    queue->SubmitSoftBody(
        m_mesh.get(), m_material.getPtr(),
        m_particleBuffer->GetBuffer(), PBufSizeAligned,
        m_totalParticleCount, m_particleSettings.general.particleAmount,
        transform, /*isDebug=*/false);

    // Debug billboard instancing (one cube per particle)
    if (m_drawDebug && m_billboardMaterial && m_billboardMesh)
    {
        queue->SubmitSoftBody(
            m_billboardMesh.getPtr(), m_billboardMaterial.getPtr(),
            m_particleBuffer->GetBuffer(), PBufSizeAligned,
            m_totalParticleCount, m_particleSettings.general.particleAmount,
            transform, /*isDebug=*/true);
    }
}

void GPUSoftBodyComponent::OnDestroy()
{
    Engine::Get()->GetRenderer()->WaitForGPU();

    if (m_particleBuffer) m_particleBuffer->Cleanup();
}

void GPUSoftBodyComponent::CreateParticleBuffers()
{
    auto renderer = Engine::Get()->GetRenderer();
    auto device = renderer->GetDevice();

    renderer->WaitForGPU();

    if (m_particleBuffer)
        m_particleBuffer->Cleanup();
    
    if (m_particles.empty())
        InitializeParticleData(m_particles, m_connections);

    VkDeviceSize PBufSize = sizeof(SBParticleData) * m_particles.size();
    VkDeviceSize CBufSize = sizeof(ConnectionData) * m_connections.size();
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

    stagingBuffer->CopyData(m_particles.data(), PBufSize);
    if (CBufSize)
        stagingBuffer->CopyData(m_connections.data(), CBufSize, PBufSizeAligned);

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

    m_particleBuffer = std::move(particleBuffer);
    stagingBuffer->Cleanup();

    std::vector<WeightedVertex> vertices;
    std::vector<uint32_t> indices;
    if (m_initializerMesh)
    {
        static_assert(offsetof(Vertex, position) == offsetof(WeightedVertex, position));
        static_assert(offsetof(Vertex, texCoord) == offsetof(WeightedVertex, texCoord));
        static_assert(offsetof(Vertex, normal) == offsetof(WeightedVertex, normal));
        static_assert(offsetof(Vertex, tangent) == offsetof(WeightedVertex, tangent));

        const uint32_t stride = (m_initializerMesh->m_isWeighted ? sizeof(WeightedVertex) : sizeof(Vertex)) / sizeof(float);
        const uint32_t vertCount = static_cast<uint32_t>(m_initializerMesh->m_vertices.size() / stride);
        const uint32_t dataStride = sizeof(Vertex) / sizeof(float);

        vertices.resize(vertCount);
        const float *ptrSource = m_initializerMesh->m_vertices.data();
        float *ptrDest = reinterpret_cast<float*>(vertices.data());

        for (uint32_t i = 0; i < vertCount; i++)
        {
            ASSERT(ptrSource < m_initializerMesh->m_vertices.data() + m_initializerMesh->m_vertices.size());
            ASSERT(reinterpret_cast<WeightedVertex*>(ptrDest) < vertices.data() + vertices.size());

            std::copy(ptrSource, ptrSource + dataStride, ptrDest);
            ptrSource += stride;
            ptrDest += sizeof(WeightedVertex) / sizeof(float);
        }

        indices = m_initializerMesh->m_indices;
    }
    else
        CreateSkinnedMesh(vertices, indices);

    MapMeshToParticles(vertices);

    m_mesh->CreateFrom(reinterpret_cast<float*>(vertices.data()), static_cast<uint32_t>(vertices.size()), indices.data(), 
        static_cast<uint32_t>(indices.size()), true);
    
    m_particles.clear();
    m_connections.clear();
}

const uint32_t _cubeSwizzlesValues[] =
{
    2, 0, 1,
    1, 2, 0,
    0, 1, 2,
    2, 1, 0,
    0, 2, 1,
    1, 0, 2
};

void GPUSoftBodyComponent::CreateSkinnedMesh(std::vector<WeightedVertex> &vertices, std::vector<uint32_t> &indices)
{
    if (m_particleSettings.shape.type == BodySettings::Shape::Type::Cube)
    {
        for (int32_t i = 0; i < 6; i++)
        {
            for (int32_t j = 0; j < m_particleSettings.general.surfacePoints.x; j++)
            {
                for (int32_t k = 0; k < m_particleSettings.general.surfacePoints.y; k++)
                {
                                        
                    Vec3f pos = Vec3f(   static_cast<float>(j) / static_cast<float>(m_particleSettings.general.surfacePoints.x - 1),
                                         static_cast<float>(k) / static_cast<float>(m_particleSettings.general.surfacePoints.y - 1),
                                         i < 3 ? -1.f : 1.f);
                    pos.x = pos.x * 2 - 1;
                    pos.y = pos.y * 2 - 1;
                    const uint32_t *ptr = _cubeSwizzlesValues + (i*3);
                    pos = Vec3f(pos[ptr[0]], pos[ptr[1]], pos[ptr[2]]);
                    WeightedVertex v;
                    v.position = pos;
                    v.normal = Vec3f(0);
                    v.normal[i % 3] = i < 3 ? -1.0f : 1.0f;
                    v.tangent = Vec3f(i % 3 == 0, i % 3 == 1, i % 3 == 2);
                    if (i >= 3) v.tangent = Vec3f(1) - v.tangent;

                    vertices.push_back(v);
                }
            }
        }

        for (int32_t i = 0; i < 6; i++)
        {
            const int32_t offset = m_particleSettings.general.surfacePoints.x * m_particleSettings.general.surfacePoints.y * i;
            for (int32_t j = 0; j < m_particleSettings.general.surfacePoints.x-1; j++)
            {
                for (int32_t k = 0; k < m_particleSettings.general.surfacePoints.y-1; k++)
                {
                    indices.push_back(offset + k * m_particleSettings.general.surfacePoints.x + j);
                    indices.push_back(offset + k * m_particleSettings.general.surfacePoints.x + j + 1);
                    indices.push_back(offset + (k + 1) * m_particleSettings.general.surfacePoints.x + j);

                    indices.push_back(offset + (k + 1) * m_particleSettings.general.surfacePoints.x + j);
                    indices.push_back(offset + k * m_particleSettings.general.surfacePoints.x + j + 1);
                    indices.push_back(offset + (k + 1) * m_particleSettings.general.surfacePoints.x + j + 1);
                }
            }
        }
    }
}

void GPUSoftBodyComponent::MapMeshToParticles(std::vector<WeightedVertex> &vertices)
{
    for (uint32_t i = 0; i < vertices.size(); i++)
    {
        struct ParticleDist
        {
            Vec3f pos;
            uint32_t id;
            float dist;
        };

        Vec3f pos = vertices[i].position;

        std::array<ParticleDist, 3> closests = std::array<ParticleDist, 3>();
        uint32_t count = 0;
        for (uint32_t l = 0; l < m_particles.size(); l++)
        {
            Vec3f pos2 = m_particles[l].position;
            float d = pos2.Distance(pos);
            if (count < closests.size())
            {
                ParticleDist p;
                p.id = l;
                p.dist = d;
                p.pos = pos2;
                closests[count] = p;
                count++;
                continue;
            }
            for (uint32_t m = 0; m < count; m++)
            {
                if (d < closests[m].dist)
                {
                    for (uint32_t n = 1; n < count-m; n++)
                    {
                        closests[count-n] = closests[count-n-1];
                    }
                    closests[m].id = l;
                    closests[m].dist = d;
                    closests[m].pos = pos2;
                    break;
                }
            }
        }

        Vec3f weights = Vec3f();
        Vec3f normal = (closests[1].pos - closests[0].pos).Cross(closests[2].pos - closests[0].pos);
        float area = normal.Length();
        area = std::copysign(area, normal.x * normal.y * normal.z);
        if (std::abs(area) > 0.001f)
        {
            normal = normal.GetNormalize();
            Vec3f pos2 = pos - normal * normal.Dot(pos - closests[0].pos);

            for (uint32_t l = 0; l < 3; l++)
            {
                Vec3f normal1 = (closests[(l+1)%3].pos - pos2).Cross(closests[(l+2)%3].pos - pos2);
                float area1 = normal1.Length();
                area1 = std::copysign(area1, normal1.x * normal1.y * normal1.z);
                float w = std::max(area1 / area, 0.0f);
                weights[l] = w;
            }
        }
        else
        {
            weights = Vec4f(1.0f, 0, 0, 0);
        }
        float l = weights[0] + weights[1] + weights[2];
        if (l <= 0.0001f)
            vertices[i].weights = Vec4f(1.0f, 0.0f, 0.0f, 0.0f);
        else
            vertices[i].weights = Vec4f(weights / l, 0.0f);
        vertices[i].indices = Vec4i(closests[0].id, closests[1].id, closests[2].id, -1);
    }
}

void GPUSoftBodyComponent::InitializeParticleData(std::vector<SBParticleData> &particles, std::vector<ConnectionData> &connections)
{
    const Vec3i amount = m_particleSettings.general.particleAmount;
    const int32_t maxL = m_particleSettings.general.connectionStrength;

    m_totalParticleCount = amount.x * amount.y * amount.z;
    particles.resize(m_totalParticleCount);

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
                particles[index].originalPos = pos;
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

        for (uint32_t i = 0; i < m_totalParticleCount; i++)
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
    m_totalParticleCount = uint32_t(particles.size());
}

void GPUSoftBodyComponent::ApplySettings()
{
    m_needsRecreation = true;
}

void GPUSoftBodyComponent::InitializeParticleDataFromMesh(float density, float maxDistToConnect)
{
    m_particles.clear();
    m_connections.clear();

    int itConnectionOffset = 0;
    UNUSED(itConnectionOffset);

    BoundingBox BBox = m_initializerMesh.getPtr()->m_boundingBox;

    constexpr size_t vertexSize = sizeof(Vertex) / sizeof(float);

    int pointCount = static_cast<int>(m_initializerMesh->m_vertices.size() / vertexSize);

    Vertex* vertices = reinterpret_cast<Vertex*>(m_initializerMesh->m_vertices.data());

    float invDensity = 1 / density;

    // Place point inside mesh
    for (float currY = BBox.min.y; currY <= BBox.max.y; currY += invDensity)
    {
        for (float currZ = BBox.min.z; currZ <= BBox.max.z; currZ += invDensity)
        {
            for (float currX = BBox.min.x; currX <= BBox.max.x; currX += invDensity)
            {

                Vec3f pos = { currX, currY, currZ };

                bool shouldDiscard = false;
                for (int i = 0; i < pointCount / 3; i++)
                {
                    Vec3f a = vertices[i * 3    ].position;
                    Vec3f b = vertices[i * 3 + 1].position;
                    Vec3f c = vertices[i * 3 + 2].position;

                    Vec3f n = (b - a).Cross(c - a);

                    Vec3f offset = pos - a;

                    if (n.Dot(offset) > 0)
                    {
                        shouldDiscard = true;
                        break;
                    }
                }
                
                if (shouldDiscard)
                    continue;

                SBParticleData data = { };

                data.position = pos;
                data.originalPos = pos;
                data.velocity = { 0 , 0 , 0 };
                data.connectionsCount = 0;
                m_particles.push_back(data);
            }
        }
    }

    // Generate connection
    for (int i = 0; i < m_particles.size(); i++)
    {
        m_particles[i].connectionsOffset = static_cast<uint32_t>(m_connections.size());
        if (m_particles[i].position.y <= BBox.min.y + 0.2f) continue;

        for (int j = 0; j < m_particles.size(); j++)
        {
            if (i == j) continue;

            float dist = m_particles[j].position.Distance(m_particles[i].position);

            if (dist <= maxDistToConnect)
            {
                ConnectionData connectionData;

                connectionData.initialLength = dist;
                connectionData.particleID = j;

                m_connections.push_back(connectionData);
            }
        }

        m_particles[i].connectionsCount = static_cast<uint32_t>(m_connections.size() - m_particles[i].connectionsOffset);
    }

    m_totalParticleCount = static_cast<uint32_t>(m_particles.size());
}