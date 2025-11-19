#include "Mesh.h"

#include "Render/RHI/RHIRenderer.h"
#include "Render/Vulkan/VulkanIndexBuffer.h"
#include "Render/Vulkan/VulkanVertexBuffer.h"

bool Mesh::Load(ResourceManager* resourceManager)
{
    return true;
}

bool Mesh::SendToGPU(RHIRenderer* renderer)
{
    // Calculate buffer sizes
    VkDeviceSize vertexBufferSize = m_vertices.size() * sizeof(float);
    VkDeviceSize indexBufferSize = m_indices.size() * sizeof(uint32_t);
    
    // Create vertex buffer
    uint32_t floatsPerVertex = 11;
    m_vertexBuffer = renderer->CreateVertexBuffer(
        m_vertices.data(), 
        static_cast<uint32_t>(m_vertices.size()), 
        floatsPerVertex
    );
    
    if (!m_vertexBuffer)
    {
        std::cerr << "Failed to create vertex buffer for mesh!" << std::endl;
        return false;
    }
    
    std::vector<uint32_t> sequentialIndices(m_vertices.size() / 11); // 11 floats per vertex
    for (uint32_t i = 0; i < sequentialIndices.size(); i++) {
        sequentialIndices[i] = i;
    }
    m_indexBuffer = renderer->CreateIndexBuffer(
        sequentialIndices.data(), 
        static_cast<uint32_t>(sequentialIndices.size())
    );
    
    if (!m_indexBuffer)
    {
        std::cerr << "Failed to create index buffer for mesh!" << std::endl;
        m_vertexBuffer.reset();
        return false;
    }
        
    return true;
}

void Mesh::Unload()
{
}
