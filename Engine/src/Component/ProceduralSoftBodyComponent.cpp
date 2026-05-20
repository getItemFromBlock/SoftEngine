#include "ProceduralSoftBodyComponent.h"
#include "Core/Engine.h"

#include "Render/Vulkan/VulkanIndexBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/VulkanVertexBuffer.h"

#include "Resource/Mesh.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"
#include "Utils/Random.h"
#include "Utils/Memory.h"

#define MAX_PARTICLE_COUNT 0x100000
#define MAX_CONNECTION_COUNT 0x1000000
#define MAX_CONNECTIONL_COUNT 0x100000
#define MAX_CHUNK_BUFFER_SIZE 0x10
#define MAX_CHUNK_BUFFER_COUNT 0x40
#define CHUNK_SIZE 4.0f

using namespace ProceduralSoftBody;

void ProceduralSoftBodyComponent::Describe(ClassDescriptor& d)
{
    d.AddVec2i("Block Size", m_particleSettings.general.particleAmount).SetRangeInt(3, 1024);
    d.AddProperty("Connections Amount", PropertyType::Int, &m_particleSettings.general.connectionStrength)
        .SetRangeInt(2, 256);
    d.AddFloat("Damping", m_particleSettings.general.damping).SetRangeFloat(0, 65536);
    d.AddFloat("Connection Strength", m_particleSettings.general.strength).SetRangeFloat(0, 65536);
    d.AddVec3f("Sphere pos", m_particleSettings.sphereData.position);
    d.AddFloat("Sphere radius", m_particleSettings.sphereData.radius);

    d.AddBool("Paused", m_particleSettings.general.paused);
    d.AddFloat("Deltatime", m_particleSettings.general.dtScale).SetRangeFloat(0, 65536);

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

    m_billboardMaterial = resourceManager->CreateMaterial("PSoftbodyInstancing", instancingShader);
    m_billboardMaterial->SetAttribute("albedoSampler", resourceManager->GetBlankTexture());
    
    m_material = resourceManager->CreateMaterial("PSoftbodySkinned", skinnedShader);
    m_material->SetAttribute("albedoSampler", resourceManager->GetBlankTexture());
    m_material->SetAttribute("normalSampler", resourceManager->GetDefaultNormal());
    m_material->SetAttribute("roughnessSampler", resourceManager->GetBlankTexture());
    m_material->SetAttribute("metalnessSampler", resourceManager->GetBlankTexture());
    m_material->SetAttribute("aoSampler", resourceManager->GetBlankTexture());
    m_material->SetAttribute("heightSampler", resourceManager->GetBlackTexture());

    m_material->SetAttribute("material.color", Vec4f::One());
    m_material->SetAttribute("material.roughnessFactor", 0.1f);
    m_material->SetAttribute("material.metalnessFactor", 1.0f);
    m_material->SetAttribute("material.aoFactor", 1.f);
    m_material->SetAttribute("material.heightScale", 0.0f);

    m_billboardMesh = resourceManager->Load<Mesh>(RESOURCE_PATH"/models/Cube.obj/Cube.mesh");
    m_sphereMesh = resourceManager->Load<Mesh>(RESOURCE_PATH"/models/Sphere.obj/Sphere.mesh");
    m_sphereMaterial = { resourceManager->Load<Material>(RESOURCE_PATH"/materials/pbr.mat") };

    computeShader0->EOnSentToGPU.Bind([this, computeShader0, renderer]()
        {
            m_simulationCompute0 = computeShader0->CreateDispatch(renderer);
        });

    computeShader1->EOnSentToGPU.Bind([this, computeShader1, renderer]()
        {
            m_simulationCompute1 = computeShader1->CreateDispatch(renderer);
        });

    CreateParticleBuffers();

    for (int i = -5; i <= 5; i++)
    {
        for (int j = -5; j <= 5; j++)
        {
            CreateChunkAt(Vec2i(i, j));
        }
    }
}

void ProceduralSoftBodyComponent::OnUpdate(float deltaTime)
{
    if (!m_simulationCompute0 || !m_simulationCompute1 || !m_particleBuffer.GetSize())
        return;

    Vec3f cameraPos = Engine::Get()->GetSceneHolder()->GetCurrentScene()->GetCameraData().position;

    /*
    for (auto &chunk : m_chunks)
    {
        Vec3f delta = chunk.second.localPosition - cameraPos;
        delta.y = 0;
        if (delta.Length() > 10.0f)
        {
            DeleteChunk(chunk.second.iPos);
        }
    }
    */
    auto renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer cmd = renderer->GetCommandBuffer();

    if (!copyRequests.empty())
    {
        const size_t stride = copyRequests.size();
        VkBufferCopy *regions = reinterpret_cast<VkBufferCopy*>(malloc(sizeof(VkBufferCopy) * stride * 3));
        for (uint32_t i = 0; i < copyRequests.size(); i++)
        {
            regions[i].size = copyRequests[i].sizeP;
            regions[i + stride].size = copyRequests[i].sizeC;
            regions[i + 2 * stride].size = copyRequests[i].sizeL;
            regions[i].srcOffset = copyRequests[i].offsetP;
            regions[i + stride].srcOffset = copyRequests[i].offsetC;
            regions[i + 2 * stride].srcOffset = copyRequests[i].offsetL;
            regions[i].dstOffset = copyRequests[i].offsetP;
            regions[i + stride].dstOffset = copyRequests[i].offsetC;
            regions[i + 2 * stride].dstOffset = copyRequests[i].offsetL;
        }
        vkCmdCopyBuffer(cmd, m_particleBuffer.GetStagingBuffer(), m_particleBuffer.GetBuffer(), (uint32_t)copyRequests.size(), regions);
        vkCmdCopyBuffer(cmd, m_connectionBuffer.GetStagingBuffer(), m_connectionBuffer.GetBuffer(), (uint32_t)copyRequests.size(), regions + copyRequests.size());
        vkCmdCopyBuffer(cmd, m_connectionBufferL.GetStagingBuffer(), m_connectionBufferL.GetBuffer(), (uint32_t)copyRequests.size(), regions + copyRequests.size()*2);

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

        free(regions);
        copyRequests.clear();
    }

    VulkanMaterial* mat0 = m_simulationCompute0->GetMaterial();
    VulkanMaterial* mat1 = m_simulationCompute1->GetMaterial();

    const Vec3f gravity = GetGameObject()->GetTransform()->GetWorldRotation().GetInverse() * Vec3f(0, 9.81f, 0);
    const Vec3f spherePos = m_particleSettings.sphereData.position;
    const float sphereRad = m_particleSettings.sphereData.radius;
    const float dt = m_particleSettings.general.paused ? 0 : std::min(deltaTime * m_particleSettings.general.dtScale, 1 / 60.0f);
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
        for (int i = -1; i <= 1; i++)
        {
            for (int j = -1; j <= 1; j++)
            {
                if (i == 0 && j == 0)
                    continue;

                const auto &neighbor = m_chunks.find(source.iPos + Vec2i(i, j));
                if (neighbor != m_chunks.end())
                {
                    tempData[count].neighbors[index].pos = neighbor->second.localPosition;
                    tempData[count].neighbors[index].offset = neighbor->second.globalOffsetP;

                }
                else
                {
                    tempData[count].neighbors[index].pos = Vec3f((source.iPos.x + i) * CHUNK_SIZE, 0.0f, (source.iPos.y + j) * CHUNK_SIZE);
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
    mat0->SetStorageBuffer( 0, 3, m_chunkDataBuffer.GetBuffer(), 0,
        m_chunkSize * particleCounts.size(), renderer);

    for (uint32_t i = 0; i < particleCounts.size(); i++)
    {
        mat0->SetPushConstants(renderer, &i, sizeof(uint32_t), 0);

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
    mat1->SetStorageBuffer( 0, 3, m_chunkDataBuffer.GetBuffer(), 0,
        m_chunkSize * particleCounts.size(), renderer);

    for (uint32_t i = 0; i < particleCounts.size(); i++)
    {
        mat1->SetPushConstants(renderer, &i, sizeof(uint32_t), 0);

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

    m_chunkBufferOffset = 0;
}

void ProceduralSoftBodyComponent::OnRender(VulkanRenderer* renderer)
{
    if (!m_particleBuffer.GetSize() || !m_material) 
        return;

    auto* rqm = Engine::Get()->GetRenderer()->GetRenderQueueManager();
    auto* queue = rqm->GetOpaqueQueue();

    const Vec3f position = p_gameObject->GetTransform()->GetWorldPosition();

    for (const auto& chunk : m_chunks)
    {
        const CPUChunkData &source = chunk.second;
        RenderCommand::ChunkRenderData data;
        data.chunkPos = source.localPosition + position;
        data.offset = source.globalOffsetP;
        uint32_t index = 0;
        for (int i = -1; i < 2; i++)
        {
            for (int j = -1; j < 2; j++)
            {
                if (i == 0 && j == 0)
                    continue;
                const auto &neighbor = m_chunks.find(source.iPos + Vec2i(i, j));
                if (neighbor != m_chunks.end())
                {
                    data.neighbors[index].pos = neighbor->second.localPosition + position;
                    data.neighbors[index].offset = neighbor->second.globalOffsetP;

                }
                else
                {
                    // Making sure that particles depending on this chunk can still compute forces properly using the originalPos field of the connection
                    data.neighbors[index].pos = Vec3f((source.iPos.x + i) * CHUNK_SIZE, 0.0f, (source.iPos.y + j) * CHUNK_SIZE) + position;
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
                data, source.particleCount, true);
        }
        else
        {
            queue->SubmitSoftBodyChunk(
                source.mesh, m_material.getPtr(),
                m_particleBuffer.GetBuffer(), m_particleBuffer.GetSize(),
                data, 1, false);
        }
    }

    Mat4 worldMat = Mat4::CreateTranslationMatrix(m_particleSettings.sphereData.position + position) * Mat4::CreateScaleMatrix(Vec3f(m_particleSettings.sphereData.radius));
    queue->SubmitMeshRenderer(worldMat, m_sphereMesh.getPtr(), m_sphereMaterial);
}

void ProceduralSoftBodyComponent::OnDestroy()
{
    Engine::Get()->GetRenderer()->WaitForGPU();

    for (const auto &chunk : m_chunks)
    {
        chunk.second.mesh->Unload();
        delete chunk.second.mesh;
    }
    m_chunks.clear();

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

    VkDeviceSize alignedSizeP = Memory::align(MAX_PARTICLE_COUNT * sizeof(PSBParticleData), m_atomicBufferAlignement);
    VkDeviceSize alignedSizeC = Memory::align(MAX_CONNECTION_COUNT * sizeof(PConnectionData0), m_atomicBufferAlignement);
    VkDeviceSize alignedSizeL = Memory::align(MAX_CONNECTIONL_COUNT * sizeof(PConnectionData1), m_atomicBufferAlignement);
    m_particleBuffer.Initialize(device, alignedSizeP);
    m_connectionBuffer.Initialize(device, alignedSizeC);
    m_connectionBufferL.Initialize(device, alignedSizeL);

    //m_chunkSize = (uint32_t)Memory::align(sizeof(GPUCommonData) + sizeof(GPUChunkData) * MAX_CHUNK_BUFFER_SIZE, m_atomicBufferAlignement);
    m_chunkSize = (uint32_t)(sizeof(GPUCommonData) + sizeof(GPUChunkData) * MAX_CHUNK_BUFFER_SIZE);
    m_chunkDataBuffer.Initialize(device, m_chunkSize * MAX_CHUNK_BUFFER_COUNT, false);
    
    m_chunkBufferOffset = 0;
}

void ProceduralSoftBodyComponent::CreateSkinnedMesh(CPUChunkData &data)
{
    data.mesh = new Mesh("internal");

    const Vec2i amount = m_particleSettings.general.particleAmount;
    const float posDelta = 1.0f / std::max(amount.x, amount.y);
    std::vector<WeightedVertex> vertices;
    std::vector<uint32_t> indices;

    for (int32_t i = 0; i <= amount.x; i++)
    {
        float posX = (i + 0.5f) / amount.x * CHUNK_SIZE;

        for (int32_t j = 0; j <= amount.y; j++)
        {
            const float posZ = (j + 0.5f) / amount.y * CHUNK_SIZE;
            const float height = GetHeightAt(posX + data.iPos.x * CHUNK_SIZE, posZ + data.iPos.y * CHUNK_SIZE);
            const Vec3f pos = Vec3f(posX, height, posZ);

            WeightedVertex v = {};
            v.position = pos;
            v.texCoord = Vec2f((float)(i) / amount.x, (float)(j) / amount.y);
            v.normal = GetNormalAt(pos + Vec3f(data.iPos.x, 0, data.iPos.y) * CHUNK_SIZE, posDelta);

            // Temporarily stores particle index in the indices for later use
            v.indices = Vec4i(i, j, 0, 0);
            vertices.push_back(v);
        }
    }

    const int stride = amount.x + 1;
    for (int32_t j = 0; j < amount.x; j++)
    {
        for (int32_t k = 0; k < amount.y; k++)
        {
            indices.push_back(j * stride + k);
            indices.push_back(j * stride + k + 1);
            indices.push_back((j + 1) * stride + k);

            indices.push_back((j + 1) * stride + k);
            indices.push_back(j * stride + k + 1);
            indices.push_back((j + 1) * stride + k + 1);
        }
    }

    MapMeshToParticles(data, vertices);

    data.mesh->CreateFrom(reinterpret_cast<float *>(vertices.data()), static_cast<uint32_t>(vertices.size()), indices.data(),
        static_cast<uint32_t>(indices.size()), true);
}

void ProceduralSoftBodyComponent::MapMeshToParticles(CPUChunkData &data, std::vector<WeightedVertex> &vertices)
{
    const Vec2i amount = m_particleSettings.general.particleAmount;
    const PreChunkData *maps[9];

    uint32_t counter = 0;
    for (int i = -1; i < 2; i++)
    {
        for (int j = -1; j < 2; j++)
        {
            Vec2i iPos = data.iPos + Vec2i(i, j);
            if (!m_preChunkData.contains(iPos))
                PreGenChunk(iPos);
            maps[counter++] = &m_preChunkData[iPos];
        }
    }

    for (uint32_t i = 0; i < vertices.size(); i++)
    {
        Vec3f pos = vertices[i].position;
        Vec2i iPos = Vec2i(vertices[i].indices.x, vertices[i].indices.y);
        int chunk = ((iPos.x + amount.x) / amount.x) * 3 + ((iPos.y + amount.y) / amount.y);

        int sx = (iPos.x + amount.x) % amount.x;
        int sz = (iPos.y + amount.y) % amount.y;

        Vec3i pPos = Vec3i(sx, maps[chunk]->heightMap[sx * amount.y + sz], sz);
        vertices[i].normal = pos;
        vertices[i].position = Vec3f(vertices[i].position.y - (pPos.y * CHUNK_SIZE / std::max(amount.x, amount.y)), maps[chunk]->positionsMap.at(pPos), chunk == 4 ? -1 : (chunk < 4 ? chunk : chunk - 1));

        for (uint32_t j = 0; j < 4; j++)
        {
            int dx = j < 2;
            int dz = j >= 2;
            if ((j & 0x1) == 0)
            {
                dx = -dx;
                dz = -dz;
            }

            int nx = 0;
            int nz = 0;
            dx += iPos.x;
            dz += iPos.y;
            if (dx < 0)
            {
                nx = -1;
                dx += amount.x;
            }
            else if (dx >= amount.x)
            {
                nx = 1;
                dx -= amount.x;
            }
            if (dz < 0)
            {
                nz = -1;
                dz += amount.y;
            }
            else if (dz >= amount.y)
            {
                nz = 1;
                dz -= amount.y;
            }

            int id = (nx + 1) * 3 + (nz + 1);
            int heightVal = maps[id]->heightMap[dx * amount.y + dz];
            vertices[i].neightbor[j] = id == 4 ? -1 : (id < 4 ? id : id - 1);
            vertices[i].indices[j] = maps[id]->positionsMap.at(Vec3i(dx, heightVal, dz));
            vertices[i].tangent[j] = GetHeightAt(((float)(dx) / amount.x + data.iPos.x + nx) * CHUNK_SIZE, ((float)(dz) / amount.y + data.iPos.y + nz) * CHUNK_SIZE) - (heightVal * CHUNK_SIZE / std::max(amount.x, amount.y));
        }
    }
}

void ProceduralSoftBodyComponent::InitializeParticleData(   std::vector<PSBParticleData> &particles, std::vector<PConnectionData0> &connections0,
                                                            std::vector<PConnectionData1> &connections1, const Vec2i &chunkID)
{
    const int32_t maxL = m_particleSettings.general.connectionStrength;

    if (!m_preChunkData.contains(chunkID))
        PreGenChunk(chunkID);
    const ProceduralSoftBody::PreChunkData &data = m_preChunkData[chunkID];

    particles.resize(data.positions.size());

    for (const auto &particleID : data.positionsMap)
    {
        auto &particle = particles[particleID.second];

        particle.position = data.positions[particleID.second];
        particle.originalPos = particle.position;
        particle.connectionsOffset = (uint32_t)connections0.size();
        particle.connectionsLOffset = (uint32_t)connections1.size();

        if (particle.originalPos.y <= 0.0f)
            continue;

        for (int l = -maxL; l <= maxL; l++)
        {
            for (int m = -maxL; m <= maxL; m++)
            {
                for (int n = -maxL; n <= maxL; n++)
                {
                    if (l == 0 && m == 0 && n == 0)
                        continue;

                    const Vec3i p = Vec3i(l, m, n) + particleID.first;
                    if (!data.positionsMap.contains(p))
                    {
                        const Vec2i amount = m_particleSettings.general.particleAmount;
                        Vec2i otherChunk;
                        Vec3i otherP = p;
                        if (p.x < 0)
                        {
                            otherChunk.x--;
                            otherP.x += amount.x;
                        }
                        else if (p.x >= amount.x)
                        {
                            otherChunk.x++;
                            otherP.x -= amount.x;
                        }
                        if (p.z < 0)
                        {
                            otherChunk.y--;
                            otherP.z += amount.y;
                        }
                        else if (p.z >= amount.y)
                        {
                            otherChunk.y++;
                            otherP.z -= amount.y;
                        }

                        if (otherP == p)
                            continue;
                        if (!m_preChunkData.contains(otherChunk + chunkID))
                            PreGenChunk(otherChunk + chunkID);
                        const ProceduralSoftBody::PreChunkData &otherChunkData = m_preChunkData[otherChunk + chunkID];
                        if (!otherChunkData.positionsMap.contains(otherP))
                            continue;

                        PConnectionData1 c;
                        c.particleID = otherChunkData.positionsMap.at(otherP);
                        c.chunkID = (otherChunk.x+1)*3+(otherChunk.y+1);
                        ASSERT(c.chunkID != 4);
                        if (c.chunkID > 4) c.chunkID--;
                        c.originalPos = otherChunkData.positions[c.particleID];
                        c.initialLength = (particle.originalPos - c.originalPos - Vec3f(otherChunk.x, 0, otherChunk.y) * CHUNK_SIZE).Length();
                        connections1.push_back(c);

                        continue;
                    }

                    const uint32_t index1 = data.positionsMap.at(p);
                    ASSERT(particleID.second != index1);

                    PConnectionData0 c;
                    c.particleID = index1;
                    c.initialLength = (particle.originalPos - data.positions[index1]).Length();
                    connections0.push_back(c);
                }
            }
        }
        particle.connectionsCount = (uint32_t)connections0.size() - particle.connectionsOffset;
        particle.connectionsLCount = (uint32_t)connections1.size() - particle.connectionsLOffset;
    }
}

void ProceduralSoftBodyComponent::CreateChunkAt(Vec2i pos)
{
    ASSERT(!m_chunks.contains(pos));

    std::vector<PSBParticleData> particles;
    std::vector<PConnectionData0> connections0;
    std::vector<PConnectionData1> connections1;
    InitializeParticleData(particles, connections0, connections1, pos);

    const uint32_t totalSizeP = (uint32_t)(sizeof(PSBParticleData) * particles.size());
    const uint32_t totalSizeC = (uint32_t)(sizeof(PConnectionData0) * connections0.size());
    const uint32_t totalSizeL = (uint32_t)(sizeof(PConnectionData1) * connections1.size());

    const BufferChunk newChunkP = AllocChunk(totalSizeP, 0);
    const BufferChunk newChunkC = AllocChunk(totalSizeC, 1);
    const BufferChunk newChunkL = AllocChunk(totalSizeL, 2);

    CPUChunkData data;
    data.pId = newChunkP.id;
    data.cId = newChunkC.id;
    data.lId = newChunkL.id;
    data.iPos = pos;
    data.globalOffsetP = newChunkP.offset / sizeof(PSBParticleData);
    data.globalOffsetC = newChunkC.offset / sizeof(PConnectionData0);
    data.globalOffsetL = newChunkL.offset / sizeof(PConnectionData1);
    data.localPosition = Vec3f(pos.x * CHUNK_SIZE, 0.0f, pos.y * CHUNK_SIZE);
    data.particleCount = (uint32_t)particles.size();
    // Sanity checks
    ASSERT(data.globalOffsetP * sizeof(PSBParticleData) == newChunkP.offset);
    ASSERT(data.globalOffsetC * sizeof(PConnectionData0) == newChunkC.offset);
    ASSERT(data.globalOffsetL * sizeof(PConnectionData1) == newChunkL.offset);

    CreateSkinnedMesh(data);

    m_particleBuffer.UpdateData(particles.data(), newChunkP.offset, totalSizeP);
    m_connectionBuffer.UpdateData(connections0.data(), newChunkC.offset, totalSizeC);
    m_connectionBufferL.UpdateData(connections1.data(), newChunkL.offset, totalSizeL);
    
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

void ProceduralSoftBodyComponent::PreGenChunk(const Vec2i &chunkID)
{
    if (m_preChunkData.contains(chunkID))
        return;

    const Vec2i amount = m_particleSettings.general.particleAmount;
    const float heightDelta = CHUNK_SIZE / std::max(amount.x, amount.y);

    PreChunkData chunkData;

    for (int32_t i = 0; i < amount.x; i++)
    {
        float posX = (i + 0.5f) / amount.x * CHUNK_SIZE;
        for (int32_t k = 0; k < amount.y; k++)
        {
            const float posZ = (k + 0.5f) / amount.y * CHUNK_SIZE;
            const float height = GetHeightAt(posX + chunkID.x * CHUNK_SIZE, posZ + chunkID.y * CHUNK_SIZE) + (heightDelta / 3);

            int j = 0;
            for (float posY = 0.0f; posY < height; posY += heightDelta)
            {
                const Vec3f pos = Vec3f(posX, posY, posZ);
                if (posY + heightDelta >= height)
                {
                    chunkData.heightMap.push_back(j);
                }
                chunkData.positionsMap[Vec3i(i, j, k)] = (uint32_t)chunkData.positions.size();
                chunkData.positions.push_back(pos);
                j++;
            }
        }
    }

    m_preChunkData[chunkID] = chunkData;
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
    return Vec2i((int)std::floor(x), (int)std::floor(y));
}

float ProceduralSoftBodyComponent::GetHeightAt(float posX, float posZ)
{
    return 1.2f + sinf(posX * 0.4687435f + 0.76543f * posZ) * 0.2f + cosf(posX * 0.61354313f - 0.2684354f * posZ) * 0.15f + sinf(-posX * 1.23384643f + 1.4687351f * posZ) * 0.1f;
}

Vec3f ProceduralSoftBodyComponent::GetNormalAt(const Vec3f &pos, float dt)
{
    float xn = GetHeightAt(pos.x - dt, pos.z);
    float xp = GetHeightAt(pos.x + dt, pos.z);
    float zn = GetHeightAt(pos.x, pos.z - dt);
    float zp = GetHeightAt(pos.x, pos.z + dt);

    ASSERT(Vec3f(xn-xp, 1.0f, zn-zp).GetNormalize().y > 0.85f);
    return Vec3f(xn-xp, 1.0f, zn-zp).GetNormalize();
}

BufferChunk ProceduralSoftBodyComponent::AllocChunk(uint32_t size, uint32_t page)
{
    ASSERT(page < 3);
    size = (uint32_t)Memory::align(size, m_atomicBufferAlignement);
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
                ASSERT(Memory::align(newChunk.size, m_atomicBufferAlignement) == newChunk.size);
                newChunk.occupied = 0;
                newChunk.id = m_globalChunkCount[page]++;
                m_memChunks[page].push_back(newChunk);
                chunk.size = size;
            }
            return chunk;
        }
    }

    BufferChunk newChunk;
    if (m_globalChunkOffset[page] + size < (page == 0 ? m_particleBuffer.GetSize() : (page == 1 ? m_connectionBuffer.GetSize() : m_connectionBufferL.GetSize())))
    {
        newChunk.offset = m_globalChunkOffset[page];
        newChunk.size = size;
        newChunk.occupied = 1;
        newChunk.id = m_globalChunkCount[page]++;
        m_memChunks[page].push_back(newChunk);
        m_globalChunkOffset[page] += size;
    }
    else
    {
        PrintError("Out of memory, bruh");
        newChunk.offset = -1;
        newChunk.size = 0;
    }

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

    PrintError("Invalid free");
}
