#include "Mesh.h"

#include "Debug/Log.h"
#include "Render/RHI/RHIRenderer.h"

bool Mesh::Load(ResourceManager* resourceManager)
{
    UNUSED(resourceManager);
    return true;
}

bool Mesh::SendToGPU(RHIRenderer* renderer)
{    
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
