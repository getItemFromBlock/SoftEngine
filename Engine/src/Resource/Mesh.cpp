#include "Mesh.h"

#include "Debug/Log.h"
#include "Render/Vulkan/VulkanRenderer.h"

class VulkanRenderer;

VkVertexInputBindingDescription Vertex::GetBindingDescription()
{
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 4> Vertex::GetAttributeDescriptions()
{
    std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions{};

    // Position
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    // Normal
    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    // Texture coordinates
    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

    // Tangent
    attributeDescriptions[3].binding = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset = offsetof(Vertex, tangent);

    return attributeDescriptions;
}

bool Mesh::Load(ResourceManager* resourceManager)
{
    UNUSED(resourceManager);
    return true;
}

bool Mesh::SendToGPU(VulkanRenderer* renderer)
{
    ASSERT(!m_vertices.empty());
    uint32_t floatsPerVertex = 11;
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

    std::vector<uint32_t> sequentialIndices(m_vertices.size() / floatsPerVertex);
    for (uint32_t i = 0; i < sequentialIndices.size(); i++)
    {
        sequentialIndices[i] = i;
    }
    m_indexBuffer = renderer->CreateIndexBuffer(
        sequentialIndices.data(),
        static_cast<uint32_t>(sequentialIndices.size())
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

std::string Mesh::GetName(bool extension) const
{
    return IResource::GetName(extension);
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
