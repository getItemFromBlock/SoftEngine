#include "Mesh.h"

#include "Core/Engine.h"
#include "ResourceManager.h"
#include "Debug/Log.h"
#include "Render/Vulkan/VulkanRenderer.h"

bool Mesh::Load(ResourceManager* resourceManager)
{   
    SafePtr<Model> model = resourceManager->Load<Model>(p_path.parent_path());
    return false; // To not send twice
}

void Mesh::CreateFrom(float *vertices, uint32_t verticeCount, uint32_t *indices, uint32_t indiceCount, bool isWeighted)
{
    p_isLoaded = false;
    p_sendToGPU = true;
    ASSERT(vertices != nullptr && verticeCount > 0 && indices != nullptr && indiceCount > 0);
    ASSERT(indiceCount % 3 == 0);

    const uint32_t verticeSize = (isWeighted ? sizeof(WeightedVertex) : sizeof(Vertex)) / sizeof(float);

    m_vertices.resize(verticeCount * verticeSize);
    std::copy(vertices, vertices + (verticeCount * verticeSize), m_vertices.data());
    m_isWeighted = isWeighted;

    SendToGPU(Engine::Get()->GetRenderer());
    p_isLoaded = true;
    p_sendToGPU = true;
}

bool Mesh::SendToGPU(VulkanRenderer* renderer)
{
    ASSERT(!m_vertices.empty());
    uint32_t floatsPerVertex = (m_isWeighted ? sizeof(WeightedVertex) : sizeof(Vertex)) / sizeof(float);
    m_vertexBuffer = renderer->CreateVertexBuffer(
        m_vertices.data(),
        static_cast<uint32_t>(m_vertices.size()),
        floatsPerVertex
    );

    if (!m_vertexBuffer)
    {
        PrintError("Failed to create vertex buffer for mesh %s", p_path.filename().generic_string().c_str());
        return false;
    }

    if (m_indices.empty())
    {
        m_indices.resize(m_vertices.size() / floatsPerVertex);
        for (uint32_t i = 0; i < m_indices.size(); i++)
        {
            m_indices[i] = i;
        }
    }

    m_indexBuffer = renderer->CreateIndexBuffer(
        m_indices.data(),
        static_cast<uint32_t>(m_indices.size())
    );

    if (!m_indexBuffer)
    {
        PrintError("Failed to create index buffer for mesh %s", p_path.generic_string().c_str());
        m_vertexBuffer.reset();
        return false;
    }

    return true;
}

void Mesh::Unload()
{
}

bool Mesh::Exists() const
{
    return File::Exist(p_path.parent_path());
}

void Mesh::ComputeBoundingBox(const std::vector<Vec3f>& positionVertices)
{
    for (const auto& vertex : positionVertices)
    {
        m_boundingBox.min.x = std::min(m_boundingBox.min.x, vertex.x);
        m_boundingBox.min.y = std::min(m_boundingBox.min.y, vertex.y);
        m_boundingBox.min.z = std::min(m_boundingBox.min.z, vertex.z);

        m_boundingBox.max.x = std::max(m_boundingBox.max.x, vertex.x);
        m_boundingBox.max.y = std::max(m_boundingBox.max.y, vertex.y);
        m_boundingBox.max.z = std::max(m_boundingBox.max.z, vertex.z);
    }
}
