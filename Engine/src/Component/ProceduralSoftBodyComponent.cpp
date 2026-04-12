#include "ProceduralSoftBodyComponent.h"
#include "Core/Engine.h"

#include "Render/Vulkan/VulkanIndexBuffer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/VulkanVertexBuffer.h"

#include "Resource/Mesh.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"
#include "Utils/Random.h"

#define MAX_BUFFER_SIZE 0x4000000
#define CHUNK_SIZE 2.0f

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
    globalChunkCount = 0;

    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();

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
    if (!m_simulationCompute0 || !m_simulationCompute1 || !m_particleBuffer)
        return;

    Vec3f cameraPos = Engine::Get()->GetSceneHolder()->GetCurrentScene()->GetCameraData().position;

    for (auto &chunk : chunks)
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

    VulkanMaterial* mat0 = m_simulationCompute0->GetMaterial();
    VulkanMaterial* mat1 = m_simulationCompute1->GetMaterial();

    // First compute pass needs both particle data and connections
    mat0->SetStorageBuffer( 0, 0, m_particleBuffer->GetBuffer(), 0,
                            PBufSizeAligned, renderer);
    mat0->SetStorageBuffer( 0, 1, m_particleBuffer->GetBuffer(), 0,
                            PBufSizeAligned, renderer);
    mat0->SetStorageBuffer( 0, 2, m_particleBuffer->GetBuffer(), 0,
                            PBufSizeAligned, renderer);

    mat0->BindForCompute(cmd, renderer->GetFrameIndex());

    struct Push0
    {
        Vec3f   gravity;
        float   deltaTime;
        float   damping;
        float   strength;
        uint32_t    offset;
        uint32_t    particleCount;
    } push0;

    push0.gravity = GetGameObject()->GetTransform()->GetWorldRotation().GetInverse() * Vec3f(0, 9.81f, 0);
    push0.deltaTime = std::min(deltaTime, 1/60.0f);
    push0.damping = m_particleSettings.general.damping;
    push0.strength = m_particleSettings.general.strength;

    for (auto &chunk : chunks)
    {

    }
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

void ProceduralSoftBodyComponent::OnRender(VulkanRenderer* renderer)
{
    if (!m_particleBuffer || !m_material) 
        return;

    auto* rqm = Engine::Get()->GetRenderer()->GetRenderQueueManager();
    auto* queue = rqm->GetOpaqueQueue();

    const Mat4 transform = GetGameObject()->GetTransform()
                               ->GetWorldMatrix().GetTranspose();
    const Vec3f spherePos = m_particleSettings.sphereData.position;
    const float sphereRad = m_particleSettings.sphereData.radius;

    for (const auto& chunk : chunks)
    {
        const CPUChunkData &source = chunk.second;
        GPUChunkData data;
        data.globalOffset = source.globalOffset;
        data.globalPos = source.localPosition;
        data.spherePos = spherePos;
        data.sphereRadius = sphereRad;
        for (uint32_t i = 0; i < 8; i++)
        {
            data.neighbors[i].offset = -1;
            data.neighbors[i].pos = Vec3f();
        }

        queue->SubmitSoftBody(
            source.mesh, m_material.getPtr(),
            m_particleBuffer->GetBuffer(), PBufSizeAligned,
            1, m_particleSettings.general.particleAmount,
            transform, false);
    }
    /*
    // Debug billboard instancing (one cube per particle)
    if (m_drawDebug && m_billboardMaterial && m_billboardMesh)
    {
        queue->SubmitSoftBody(
            m_billboardMesh.getPtr(), m_billboardMaterial.getPtr(),
            m_particleBuffer->GetBuffer(), PBufSizeAligned,
            m_totalParticleCount, m_particleSettings.general.particleAmount,
            transform, true);
    }
    */
}

void ProceduralSoftBodyComponent::OnDestroy()
{
    Engine::Get()->GetRenderer()->WaitForGPU();

    if (m_particleBuffer) m_particleBuffer->Cleanup();
}

void ProceduralSoftBodyComponent::CreateParticleBuffers()
{
    auto renderer = Engine::Get()->GetRenderer();
    auto device = renderer->GetDevice();
    ASSERT(!m_particleBuffer);

    renderer->WaitForGPU();

    VkDeviceSize PBufSize = MAX_BUFFER_SIZE;
    PBufSizeAligned = align(PBufSize, 0x40);
    PBufSizeAligned = std::max(0x40llu, PBufSize);

    auto particleBuffer = std::make_unique<VulkanBuffer>();
    particleBuffer->Initialize(device, PBufSizeAligned,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkCommandBufferAllocateInfo alloc{};
    alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandPool = renderer->GetCommandPool()->GetCommandPool();
    alloc.commandBufferCount = 1;

    m_particleBuffer = std::move(particleBuffer);
}

void ProceduralSoftBodyComponent::CreateSkinnedMesh(CPUChunkData &data, std::vector<PSBParticleData> &particles)
{
    data.mesh = new Mesh("internal");

    const std::vector<HeightPointData> *maps[4];

    uint32_t counter = 0;
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            Vec2i iPos = data.iPos + Vec2i(i, j);
            if (!heightData.contains(iPos))
                CreateHeightMap(iPos);
            maps[counter++] = &heightData[iPos];
        }
    }

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

    // TODO Map mesh to points
}

void ProceduralSoftBodyComponent::InitializeParticleData(   std::vector<PSBParticleData> &particles, std::vector<PConnectionData0> &connections0,
                                                            std::vector<PConnectionData1> &connections1, const Vec2i &chunkID)
{
    const Vec2i amount = m_particleSettings.general.particleAmount;
    const int32_t maxL = m_particleSettings.general.connectionStrength;
    const float heightDelta = 1.0f / std::max(amount.x, amount.y);

    std::unordered_map<Vec3i, uint32_t> tmpParticles;
    std::vector<HeightPointData> heightMap;
    const bool exist = heightData.contains(chunkID);

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
        heightData[chunkID] = heightMap;

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

uint32_t ProceduralSoftBodyComponent::CreateChunkAt(Vec3f pos)
{
    const Vec2i k = GetChunkPos(pos);
    ASSERT(!chunks.contains(k));

    std::vector<PSBParticleData> particles;
    std::vector<PConnectionData0> connections0;
    std::vector<PConnectionData1> connections1;
    InitializeParticleData(particles, connections0, connections1, k);

    const uint32_t totalSize = sizeof(PSBParticleData) * particles.size() + sizeof(uint32_t) * connections0.size();
    const BufferChunk newChunk = AllocChunk(totalSize);

    CPUChunkData data;
    data.id = newChunk.id;
    data.iPos = k;
    data.globalOffset = newChunk.offset;
    data.localPosition = Vec3f(k.x * CHUNK_SIZE, 0.0f, k.y * CHUNK_SIZE);

    CreateSkinnedMesh(data, particles);
    
    // TODO copy data to GPU

    chunks[k] = data;

    return data.id;
}

void ProceduralSoftBodyComponent::CreateHeightMap(const Vec2i &chunkID)
{
    const Vec2i amount = m_particleSettings.general.particleAmount;
    const float heightDelta = 1.0f / std::max(amount.x, amount.y);

    std::vector<HeightPointData> heightMap;
    uint32_t counter = 0;
    const bool exist = heightData.contains(chunkID);

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
        heightData[chunkID] = heightMap;
}

void ProceduralSoftBodyComponent::DeleteChunk(Vec2i iPos)
{
    ASSERT(chunks.contains(iPos));
    const auto &chunk = chunks.at(iPos);
    delete chunk.mesh;
    FreeChunk(chunk.id);
    chunks.erase(iPos);
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

BufferChunk ProceduralSoftBodyComponent::AllocChunk(uint32_t size)
{
    for (auto &chunk : memChunks)
    {
        if (!chunk.occupied && chunk.size >= size)
        {
            chunk.occupied = true;
            chunk.id = globalChunkCount++;

            if (size >= chunk.size + 0x400)
            {
                BufferChunk newChunk;
                newChunk.offset = chunk.offset + chunk.size;
                newChunk.size = chunk.size - size;
                newChunk.occupied = 0;
                newChunk.id = globalChunkCount++;
                memChunks.push_back(newChunk);
                chunk.size = size;
            }
            return chunk;
        }
    }

    ASSERT(globalChunkOffset + size < MAX_BUFFER_SIZE);

    BufferChunk newChunk;
    newChunk.offset = globalChunkOffset;
    newChunk.size = size;
    newChunk.occupied = 1;
    newChunk.id = globalChunkCount++;
    memChunks.push_back(newChunk);
    globalChunkOffset += size;

    return newChunk;
}

void ProceduralSoftBodyComponent::FreeChunk(uint32_t id)
{
    for (auto chunk = memChunks.begin(); chunk != memChunks.end(); chunk++)
    {
        if (chunk->id == id)
        {
            ASSERT(chunk->occupied);
            chunk->occupied = false;

            const auto prev = std::prev(chunk);
            if (prev != memChunks.begin() && !prev->occupied)
            {
                prev->size += chunk->size;
                memChunks.erase(chunk);
                chunk = prev;
            }

            const auto next = std::next(chunk);
            if (next == memChunks.end())
            {
                globalChunkOffset -= chunk->size;
                memChunks.erase(chunk);
            }
            else if (!next->occupied)
            {
                chunk->size += next->size;
                memChunks.erase(next);
            }
            return;
        }
    }

    PrintError("Bruh, out of memory");
}
