#include "ProceduralSoftBodyComponent.h"
#include "Core/Engine.h"

#include "Render/Vulkan/VulkanIndexBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/VulkanVertexBuffer.h"

#include "Resource/Mesh.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"
#include "Utils/Random.h"

#define MAX_PARTICLE_COUNT 0x100000
#define MAX_CONNECTION_COUNT 0x400000
#define MAX_CONNECTIONL_COUNT 0x40000
#define MAX_CHUNK_BUFFER_SIZE 0x10
#define MAX_CHUNK_BUFFER_COUNT 0x40
#define CHUNK_SIZE 2.0f

using namespace ProceduralSoftBody;

// Aligns an integer to the next nearest memory aligned value. Alignement MUST be a power of two!
uint64_t align(uint64_t value, uint64_t alignement)
{
    ASSERT(((alignement-1) & alignement) == 0);
    return (value + alignement - 1) & ~(alignement - 1);
}

void ProceduralSoftBodyComponent::Describe(ClassDescriptor& d)
{
    d.AddVec2i("Block Size", m_particleSettings.general.particleAmount).SetRangeInt(3, 1024);

    d.AddProperty("Connections Amount", PropertyType::Int, &m_particleSettings.general.connectionStrength)
        .SetRangeInt(2, 256);

    d.AddFloat("Damping", m_particleSettings.general.damping).SetRangeFloat(0, 65536);

    d.AddFloat("Connection Strength", m_particleSettings.general.strength).SetRangeFloat(0, 65536);

    d.AddBool("Debug", m_drawDebug);
}

void ProceduralSoftBodyComponent::OnCreate()
{
    m_globalChunkCount[0] = 0;
    m_globalChunkCount[1] = 0;
    m_globalChunkCount[2] = 0;
    m_globalChunkOffset[0] = 0;
    m_globalChunkOffset[1] = 0;
    m_globalChunkOffset[2] = 0;

    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(renderer->GetDevice()->GetPhysicalDevice(), &props);
    m_atomicBufferAlignement = std::max(props.limits.minUniformBufferOffsetAlignment, props.limits.nonCoherentAtomSize);

    auto computeShader0 = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/PSoftbodyCompute/softbody0.shader");
    auto computeShader1 = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/PSoftbodyCompute/softbody1.shader");
    auto instancingShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/PSoftbodyCompute/sb_instancing.shader");
    auto skinnedShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/PSoftbodyCompute/sb_skinning.shader");

    m_billboardMaterial = resourceManager->CreateMaterial("PSoftbodyInstancing");
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

void ProceduralSoftBodyComponent::OnUpdate(float deltaTime)
{
    if (!m_simulationCompute0 || !m_simulationCompute1 || !m_particleBuffer.GetSize())
        return;

    Vec3f cameraPos = Engine::Get()->GetSceneHolder()->GetCurrentScene()->GetCameraData().position;

    for (auto &chunk : m_chunks)
    {
        Vec3f delta = chunk.second.localPosition - cameraPos;
        delta.y = 0;
        if (delta.Length() > 10.0f)
        {
            DeleteChunk(chunk.second.iPos);
        }
    }

    auto renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer cmd = renderer->GetCommandBuffer();

    if (!copyRequests.empty())
    {
        const size_t stride = copyRequests.size() * sizeof(VkBufferCopy);
        VkBufferCopy *regions = reinterpret_cast<VkBufferCopy*>(malloc(stride * 3));
        for (uint32_t i = 0; i < copyRequests.size(); i++)
        {
            regions[i].size = copyRequests[i].sizeP;
            regions[i + stride].size = copyRequests[i].sizeC;
            regions[i + stride*2].size = copyRequests[i].sizeL;
            regions[i].srcOffset = copyRequests[i].offsetP;
            regions[i + stride].srcOffset = copyRequests[i].offsetC;
            regions[i + stride*2].srcOffset = copyRequests[i].offsetL;
            regions[i].dstOffset = copyRequests[i].offsetP;
            regions[i + stride].dstOffset = copyRequests[i].offsetC;
            regions[i + stride*2].dstOffset = copyRequests[i].offsetL;
        }
        vkCmdCopyBuffer(cmd, m_particleBuffer.GetStagingBuffer(), m_particleBuffer.GetBuffer(), copyRequests.size(), regions);
        vkCmdCopyBuffer(cmd, m_connectionBuffer.GetStagingBuffer(), m_connectionBuffer.GetBuffer(), copyRequests.size(), regions + stride);
        vkCmdCopyBuffer(cmd, m_connectionBufferL.GetStagingBuffer(), m_connectionBufferL.GetBuffer(), copyRequests.size(), regions + stride*2);

        VkBufferMemoryBarrier barriers[3] = {};
        barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[0].buffer = m_particleBuffer.GetBuffer();
        barriers[0].size = VK_WHOLE_SIZE;

        barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[1].buffer = m_connectionBuffer.GetBuffer();
        barriers[1].size = VK_WHOLE_SIZE;

        barriers[2].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[2].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[2].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[2].buffer = m_connectionBufferL.GetBuffer();
        barriers[2].size = VK_WHOLE_SIZE;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 0, nullptr, 3, barriers, 0, nullptr);
    }

    VulkanMaterial* mat0 = m_simulationCompute0->GetMaterial();
    VulkanMaterial* mat1 = m_simulationCompute1->GetMaterial();

    const Vec3f gravity = GetGameObject()->GetTransform()->GetWorldRotation().GetInverse() * Vec3f(0, 9.81f, 0);
    const Vec3f spherePos = m_particleSettings.sphereData.position;
    const float sphereRad = m_particleSettings.sphereData.radius;
    const float dt = std::min(deltaTime, 1 / 60.0f);
    const float damping = m_particleSettings.general.damping;
    const float strength = m_particleSettings.general.strength;

    GPUCommonData   commonData;
    commonData.spherePos = spherePos;
    // TODO set radius to -1 when can't collide ? Would need per chunk data tho
    commonData.sphereRadius = sphereRad;
    commonData.damping = damping;
    commonData.deltaTime = dt;
    commonData.strength = strength;
    commonData.gravity = gravity;

    GPUChunkData    tempData[MAX_CHUNK_BUFFER_SIZE];
    uint32_t    count = 0;
    uint32_t    particleCount = 0;
    std::vector<uint32_t>   particleCounts;
    uint8_t *ptr = reinterpret_cast<uint8_t*>(m_chunkDataBuffer.GetMappedBuffer());
   
    for (auto &chunk : m_chunks)
    {
        const CPUChunkData &source = chunk.second;

        tempData[count].chunkPos = source.localPosition;
        tempData[count].offset = source.globalOffsetP;
        tempData[count].connectionOffset = source.globalOffsetC;
        tempData[count].connectionLOffset = source.globalOffsetL;
        tempData[count].particleCount = source.particleCount;
        uint32_t index = 0;
        for (int i = -1; i < 2; i++)
        {
            for (int j = -1; j < 2; j++)
            {
                const auto &neighbor = m_chunks.find(source.iPos + Vec2i(i, j));
                if (neighbor != m_chunks.end())
                {
                    tempData[count].neighbors[index].pos = neighbor->second.localPosition;
                    tempData[count].neighbors[index].offset = neighbor->second.globalOffsetP;

                }
                else
                {
                    tempData[count].neighbors[index].pos = Vec3f();
                    tempData[count].neighbors[index].offset = -1;
                }
                index++;
            }
        }
        particleCount += source.particleCount;
        count++;
        if (count == MAX_CHUNK_BUFFER_SIZE)
        {
            std::memcpy(ptr + m_chunkBufferOffset * m_chunkSize, &commonData, sizeof(GPUCommonData));
            std::memcpy(ptr + m_chunkBufferOffset * m_chunkSize + sizeof(GPUCommonData), tempData, sizeof(GPUChunkData) * MAX_CHUNK_BUFFER_SIZE);
            m_chunkBufferOffset++;

            particleCounts.push_back(particleCount);
            count = 0;
            particleCount = 0;
        }
    }
    if (count)
    {
        std::memcpy(ptr + m_chunkBufferOffset * m_chunkSize, &commonData, sizeof(GPUCommonData));
        std::memcpy(ptr + m_chunkBufferOffset * m_chunkSize + sizeof(GPUCommonData), tempData, sizeof(GPUChunkData) * count);
        m_chunkBufferOffset++;
        particleCounts.push_back(particleCount);
    }

    m_chunkDataBuffer.FlushData(0, m_chunkBufferOffset * m_chunkSize);
    m_chunkDataBuffer.CopyDataToDevice(cmd, 0, m_chunkBufferOffset * m_chunkSize);

    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.buffer = m_chunkDataBuffer.GetBuffer();
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &barrier, 0, nullptr);

    mat0->BindForCompute(cmd, renderer->GetFrameIndex());

    mat0->SetStorageBuffer( 0, 0, m_particleBuffer.GetBuffer(), 0,
        m_particleBuffer.GetSize(), renderer);
    mat0->SetStorageBuffer( 0, 1, m_connectionBuffer.GetBuffer(), 0,
        m_connectionBuffer.GetSize(), renderer);
    mat0->SetStorageBuffer( 0, 2, m_connectionBufferL.GetBuffer(), 0,
        m_connectionBufferL.GetSize(), renderer);

    for (uint32_t i = 0; i < particleCounts.size(); i++)
    {
        mat0->SetStorageBuffer( 0, 3, m_chunkDataBuffer.GetBuffer(), i * m_chunkSize,
            m_chunkSize, renderer);

        uint32_t groups = (particleCounts[i] + 63) / 64;
        mat0->DispatchCompute(renderer, groups, 1, 1);
    }

    VkBufferMemoryBarrier barrier0{};
    barrier0.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier0.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier0.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier0.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier0.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier0.buffer = m_particleBuffer.GetBuffer();
    barrier0.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 1, &barrier0, 0, nullptr);

    mat1->BindForCompute(cmd, renderer->GetFrameIndex());

    mat1->SetStorageBuffer( 0, 0, m_particleBuffer.GetBuffer(), 0,
        m_particleBuffer.GetSize(), renderer);

    for (uint32_t i = 0; i < particleCounts.size(); i++)
    {
        mat1->SetStorageBuffer( 0, 3, m_chunkDataBuffer.GetBuffer(), i * m_chunkSize,
            m_chunkSize, renderer);

        uint32_t groups = (particleCounts[i] + 63) / 64;
        mat1->DispatchCompute(renderer, groups, 1, 1);
    }

    VkBufferMemoryBarrier barrier1{};
    barrier1.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier1.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier1.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier1.buffer = m_particleBuffer.GetBuffer();
    barrier1.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
        0, 0, nullptr, 1, &barrier1, 0, nullptr);
}

void ProceduralSoftBodyComponent::OnRender(VulkanRenderer* renderer)
{
    if (!m_particleBuffer.GetSize() || !m_material) 
        return;

    auto* rqm = Engine::Get()->GetRenderer()->GetRenderQueueManager();
    auto* queue = rqm->GetOpaqueQueue();

    const Mat4 transform = GetGameObject()->GetTransform()
                               ->GetWorldMatrix().GetTranspose();
    const Vec3f spherePos = m_particleSettings.sphereData.position;
    const float sphereRad = m_particleSettings.sphereData.radius;

    for (const auto& chunk : m_chunks)
    {
        const CPUChunkData &source = chunk.second;
        GPURenderData data;
        data.chunkPos = source.localPosition;
        data.offset = source.globalOffsetP;
        uint32_t index = 0;
        for (int i = -1; i < 2; i++)
        {
            for (int j = -1; j < 2; j++)
            {
                const auto &neighbor = m_chunks.find(source.iPos + Vec2i(i, j));
                if (neighbor != m_chunks.end())
                {
                    data.neighbors[index].pos = neighbor->second.localPosition;
                    data.neighbors[index].offset = neighbor->second.globalOffsetP;

                }
                else
                {
                    data.neighbors[index].pos = Vec3f();
                    data.neighbors[index].offset = -1;
                }
                index++;
            }
        }

        if (m_drawDebug && m_billboardMaterial && m_billboardMesh)
        {
            queue->SubmitSoftBodyChunk(
                m_billboardMesh.getPtr(), m_billboardMaterial.getPtr(),
                m_particleBuffer.GetBuffer(), m_particleBuffer.GetSize(),
                data, 1, transform, true);
        }
        else
        {
            queue->SubmitSoftBodyChunk(
                source.mesh, m_material.getPtr(),
                m_particleBuffer.GetBuffer(), m_particleBuffer.GetSize(),
                data, source.particleCount, transform, false);
        }
    }
}

void ProceduralSoftBodyComponent::OnDestroy()
{
    Engine::Get()->GetRenderer()->WaitForGPU();

    if (m_particleBuffer.GetSize())
    {
        m_particleBuffer.Cleanup();
        m_connectionBuffer.Cleanup();
        m_connectionBufferL.Cleanup();
        m_chunkDataBuffer.Cleanup();
    }
}

void ProceduralSoftBodyComponent::CreateParticleBuffers()
{
    auto renderer = Engine::Get()->GetRenderer();
    auto device = renderer->GetDevice();
    ASSERT(!m_particleBuffer.GetSize());

    renderer->WaitForGPU();

    VkDeviceSize alignedSizeP = align(MAX_PARTICLE_COUNT * sizeof(PSBParticleData), m_atomicBufferAlignement);
    VkDeviceSize alignedSizeC = align(MAX_PARTICLE_COUNT * sizeof(PConnectionData0), m_atomicBufferAlignement);
    VkDeviceSize alignedSizeL = align(MAX_PARTICLE_COUNT * sizeof(PConnectionData1), m_atomicBufferAlignement);
    m_particleBuffer.Initialize(device, alignedSizeP);
    m_connectionBuffer.Initialize(device, alignedSizeC);
    m_connectionBufferL.Initialize(device, alignedSizeL);

    m_chunkSize = align(sizeof(GPUCommonData) + sizeof(GPUChunkData) * MAX_CHUNK_BUFFER_SIZE, m_atomicBufferAlignement);
    m_chunkDataBuffer.Initialize(device, m_chunkSize * MAX_CHUNK_BUFFER_COUNT, false);
    
    m_chunkBufferOffset = 0;
}

void ProceduralSoftBodyComponent::CreateSkinnedMesh(CPUChunkData &data)
{
    data.mesh = new Mesh("internal");

    const Vec2i amount = m_particleSettings.general.particleAmount;
    const Vec2i numPoints = m_particleSettings.general.surfacePoints;
    const float posDelta = 1.0f / std::max(numPoints.x, numPoints.y);
    std::vector<WeightedVertex> vertices;
    std::vector<uint32_t> indices;

    for (int32_t i = 0; i <= numPoints.x; i++)
    {
        float posX = (float)(i) / numPoints.x + 0.5f / amount.x;

        for (int32_t j = 0; j <= numPoints.y; j++)
        {
            const float posZ = (float)(j) / numPoints.y + 0.5f / amount.y;
            const float height = GetHeightAt(posX + data.iPos.x * CHUNK_SIZE, posZ + data.iPos.y * CHUNK_SIZE);
            const Vec3f pos = Vec3f(posX, height, posZ);

            WeightedVertex v = {};
            v.position = pos;
            v.texCoord = Vec2f((float)(i) / numPoints.x, (float)(j) / numPoints.y);
            v.normal = GetNormalAt(pos, 1.0f / posDelta);
            v.tangent = Vec3f::Forward().Cross(v.normal);
            vertices.push_back(v);
        }
    }

    for (int32_t j = 0; j < numPoints.x - 1; j++)
    {
        for (int32_t k = 0; k < numPoints.y - 1; k++)
        {
            indices.push_back(k * numPoints.x + j);
            indices.push_back(k * numPoints.x + j + 1);
            indices.push_back((k + 1) * numPoints.x + j);

            indices.push_back((k + 1) * numPoints.x + j);
            indices.push_back(k * numPoints.x + j + 1);
            indices.push_back((k + 1) * numPoints.x + j + 1);
        }
    }

    MapMeshToParticles(data, vertices);

    data.mesh->CreateFrom(reinterpret_cast<float *>(vertices.data()), static_cast<uint32_t>(vertices.size()), indices.data(),
        static_cast<uint32_t>(indices.size()), true);
}

void ProceduralSoftBodyComponent::MapMeshToParticles(CPUChunkData &data, std::vector<WeightedVertex> &vertices)
{
    const std::vector<HeightPointData> *maps[4];

    uint32_t counter = 0;
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            Vec2i iPos = data.iPos + Vec2i(i, j);
            if (!m_heightData.contains(iPos))
                CreateHeightMap(iPos);
            maps[counter++] = &m_heightData[iPos];
        }
    }

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

        const bool cr = pos.x < 0.5f;
        const bool cu = pos.z < 0.5f;

        for (int x = 0; x < 2; x++)
        {
            if (cr && x > 0)
                break;
            for (int y = 0; y < 2; y++)
            {
                if (cu && y > 0)
                    break;

                const std::vector<HeightPointData> &particles = *(maps[x*2+y]);
                for (uint32_t l = 0; l < particles.size(); l++)
                {
                    Vec3f pos2 = particles[l].pos;
                    float d = pos2.Distance(pos);
                    if (count < closests.size())
                    {
                        ParticleDist p;
                        p.id = particles[l].id;
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
                            for (uint32_t n = 1; n < count - m; n++)
                            {
                                closests[count - n] = closests[count - n - 1];
                            }
                            closests[m].id = particles[l].id;
                            closests[m].dist = d;
                            closests[m].pos = pos2;
                            break;
                        }
                    }
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
                Vec3f normal1 = (closests[(l + 1) % 3].pos - pos2).Cross(closests[(l + 2) % 3].pos - pos2);
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

void ProceduralSoftBodyComponent::InitializeParticleData(   std::vector<PSBParticleData> &particles, std::vector<PConnectionData0> &connections0,
                                                            std::vector<PConnectionData1> &connections1, const Vec2i &chunkID)
{
    const Vec2i amount = m_particleSettings.general.particleAmount;
    const int32_t maxL = m_particleSettings.general.connectionStrength;
    const float heightDelta = 1.0f / std::max(amount.x, amount.y);

    std::unordered_map<Vec3i, uint32_t> tmpParticles;
    std::vector<HeightPointData> heightMap;
    const bool exist = m_heightData.contains(chunkID);

    for (int32_t i = 0; i < amount.x; i++)
    {
        float posX = (i + 0.5f) / amount.x;
        for (int32_t k = 0; k < amount.y; k++)
        {
            float posZ = (k + 0.5f) / amount.y;
            float height = GetHeightAt(posX + chunkID.x * CHUNK_SIZE, posZ + chunkID.y * CHUNK_SIZE);

            int j = 0;
            for (float posY = -0.5f; posY < height; posY += heightDelta)
            {
                const Vec3f pos = Vec3f(posX, posY, posZ);
                if (!exist && posY + heightDelta >= height)
                {
                    HeightPointData d;
                    d.id = particles.size();
                    d.pos = pos;
                    heightMap.push_back(d);
                }

                PSBParticleData particle = {};
                particle.position = pos;
                particle.originalPos = pos;

                tmpParticles[Vec3i(i, j++, k)] = particles.size();
                particles.push_back(particle);
            }
        }
    }

    if (!exist)
        m_heightData[chunkID] = heightMap;

    for (auto particleID : tmpParticles)
    {
        auto &particle = particles[particleID.second];

        if (particle.originalPos.y == -0.5f)
            continue;

        particle.connectionsOffset = (uint32_t)connections0.size();

        for (int32_t l = -maxL; l <= maxL; l++)
        {
            for (int32_t m = -maxL; m <= maxL; m++)
            {
                for (int32_t n = -maxL; n <= maxL; n++)
                {
                    if (l == 0 && m == 0 && n == 0)
                        continue;

                    const Vec3f p = Vec3f(l, m, n) + particleID.first;
                    if (!tmpParticles.contains(p))
                    {
                        // TODO deal with neighbor chunk particles
                        continue;
                    }

                    const uint32_t index1 = tmpParticles[p];
                    if (particleID.second == index1)
                        continue;

                    PConnectionData0 c;
                    c.particleID = index1;
                    c.initialLength = (particle.originalPos - particles[index1].originalPos).Length();
                    connections0.push_back(c);
                }
            }
        }
        particle.connectionsCount = (uint32_t)connections0.size() - particle.connectionsOffset;
    }
}

void ProceduralSoftBodyComponent::CreateChunkAt(Vec2i pos)
{
    ASSERT(!m_chunks.contains(pos));

    std::vector<PSBParticleData> particles;
    std::vector<PConnectionData0> connections0;
    std::vector<PConnectionData1> connections1;
    InitializeParticleData(particles, connections0, connections1, pos);

    const uint32_t totalSizeP = sizeof(PSBParticleData) * particles.size();
    const uint32_t totalSizeC = sizeof(PConnectionData0) * connections0.size();
    const uint32_t totalSizeL = sizeof(PConnectionData1) * connections1.size();

    const BufferChunk newChunkP = AllocChunk(totalSizeP, 0);
    const BufferChunk newChunkC = AllocChunk(totalSizeC, 1);
    const BufferChunk newChunkL = AllocChunk(totalSizeL, 2);

    CPUChunkData data;
    data.pId = newChunkP.id;
    data.cId = newChunkC.id;
    data.lId = newChunkL.id;
    data.iPos = pos;
    data.globalOffsetP = newChunkP.offset;
    data.globalOffsetC = newChunkC.offset;
    data.globalOffsetL = newChunkL.offset;
    data.localPosition = Vec3f(pos.x * CHUNK_SIZE, 0.0f, pos.y * CHUNK_SIZE);

    CreateSkinnedMesh(data);

    m_particleBuffer.UpdateData(particles.data(), newChunkP.offset, particles.size() * sizeof(PSBParticleData));
    m_particleBuffer.UpdateData(particles.data(), newChunkP.offset, particles.size() * sizeof(PSBParticleData));
    m_particleBuffer.UpdateData(particles.data(), newChunkP.offset, particles.size() * sizeof(PSBParticleData));
    
    CopyRequest cr = {};
    cr.offsetP = newChunkP.offset;
    cr.sizeP = newChunkP.size;
    cr.offsetC = newChunkC.offset;
    cr.sizeC = newChunkC.size;
    cr.offsetL = newChunkL.offset;
    cr.sizeL = newChunkL.size;
    copyRequests.push_back(cr);

    m_chunks[pos] = data;
}

void ProceduralSoftBodyComponent::CreateHeightMap(const Vec2i &chunkID)
{
    const Vec2i amount = m_particleSettings.general.particleAmount;
    const float heightDelta = 1.0f / std::max(amount.x, amount.y);

    std::vector<HeightPointData> heightMap;
    uint32_t counter = 0;
    const bool exist = m_heightData.contains(chunkID);

    for (int32_t i = 0; i < amount.x; i++)
    {
        float posX = (i + 0.5f) / amount.x;
        for (int32_t k = 0; k < amount.y; k++)
        {
            const float posZ = (k + 0.5f) / amount.y;
            const float height = GetHeightAt(posX + chunkID.x * CHUNK_SIZE, posZ + chunkID.y * CHUNK_SIZE);

            for (float posY = -0.5f; posY < height; posY += heightDelta)
            {
                const Vec3f pos = Vec3f(posX, posY, posZ);
                if (!exist && posY + heightDelta >= height)
                {
                    HeightPointData d;
                    d.id = counter;
                    d.pos = pos;
                    heightMap.push_back(d);
                }
                counter++;
            }
        }
    }

    if (!exist)
        m_heightData[chunkID] = heightMap;
}

void ProceduralSoftBodyComponent::DeleteChunk(Vec2i iPos)
{
    ASSERT(m_chunks.contains(iPos));
    const auto &chunk = m_chunks.at(iPos);
    delete chunk.mesh;
    FreeChunk(chunk.pId, 0);
    FreeChunk(chunk.cId, 1);
    FreeChunk(chunk.lId, 2);
    m_chunks.erase(iPos);
}

Vec2i ProceduralSoftBodyComponent::GetChunkPos(Vec3f pos)
{
    const float x = pos.x / CHUNK_SIZE;
    const float y = pos.y / CHUNK_SIZE;
    return Vec2i(std::floor(x), std::floor(y));
}

float ProceduralSoftBodyComponent::GetHeightAt(float posX, float posZ)
{
    return sinf(posX * 0.4687435f + 0.76543f * posZ) * 0.25f + cosf(posX * 0.61354313 - 0.2684354 * posZ) * 0.2f + sinf(-posX * 1.23384643f + 1.4687351f * posZ) * 0.15f;
}

Vec3f ProceduralSoftBodyComponent::GetNormalAt(const Vec3f &pos, float dt)
{
    Vec2f xp = Vec2f(pos.x + dt, GetHeightAt(pos.x + dt, pos.z));
    Vec2f xn = Vec2f(pos.x - dt, GetHeightAt(pos.x - dt, pos.z));
    Vec2f zp = Vec2f(pos.z + dt, GetHeightAt(pos.x, pos.z + dt));
    Vec2f zn = Vec2f(pos.z - dt, GetHeightAt(pos.x, pos.z - dt));
    float tmpX = xp.Length();
    float fx = tmpX / (xn.Length() + tmpX);
    float dx = xp.x * fx + xn.x * (1 - fx);
    float tmpZ = zp.Length();
    float fz = tmpZ / (zn.Length() + tmpZ);
    float dz = zp.x * fz + zn.x * (1 - fz);

    return Vec3f(pos.x - dx, 1.0f, pos.z - dz).GetNormalize();
}

BufferChunk ProceduralSoftBodyComponent::AllocChunk(uint32_t size, uint32_t page)
{
    ASSERT(page < 3);
    size = align(size, m_atomicBufferAlignement);
    for (auto &chunk : m_memChunks[page])
    {
        if (!chunk.occupied && chunk.size >= size)
        {
            chunk.occupied = true;
            chunk.id = m_globalChunkCount[page]++;

            if (chunk.size >= size + m_atomicBufferAlignement)
            {
                BufferChunk newChunk;
                newChunk.offset = chunk.offset + chunk.size;
                newChunk.size = chunk.size - size;
                ASSERT(align(newChunk.size, m_atomicBufferAlignement) == newChunk.size);
                newChunk.occupied = 0;
                newChunk.id = m_globalChunkCount[page]++;
                m_memChunks[page].push_back(newChunk);
                chunk.size = size;
            }
            return chunk;
        }
    }

    ASSERT(m_globalChunkOffset[page] + size < (page == 0 ? m_particleBuffer.GetSize() : (page == 1 ? m_connectionBuffer.GetSize() : m_connectionBufferL.GetSize())));

    BufferChunk newChunk;
    newChunk.offset = m_globalChunkOffset[page];
    newChunk.size = size;
    newChunk.occupied = 1;
    newChunk.id = m_globalChunkCount[page]++;
    m_memChunks[page].push_back(newChunk);
    m_globalChunkOffset[page] += size;

    return newChunk;
}

void ProceduralSoftBodyComponent::FreeChunk(uint32_t id, uint32_t page)
{
    for (auto chunk = m_memChunks[page].begin(); chunk != m_memChunks[page].end(); chunk++)
    {
        if (chunk->id == id)
        {
            ASSERT(chunk->occupied);
            chunk->occupied = false;

            const auto prev = std::prev(chunk);
            if (prev != m_memChunks[page].begin() && !prev->occupied)
            {
                prev->size += chunk->size;
                m_memChunks[page].erase(chunk);
                chunk = prev;
            }

            const auto next = std::next(chunk);
            if (next == m_memChunks[page].end())
            {
                m_globalChunkOffset[page] -= chunk->size;
                m_memChunks[page].erase(chunk);
            }
            else if (!next->occupied)
            {
                chunk->size += next->size;
                m_memChunks[page].erase(next);
            }
            return;
        }
    }

    PrintError("Bruh, out of memory");
}
