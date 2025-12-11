#pragma once
#include <vector>
#include <memory>
#include <array>

#include <galaxymath/Maths.h>

#include "IResource.h"

#include "Render/RHI/RHIVertex.h"
#include "Render/RHI/RHIVertexBuffer.h"
#include "Render/RHI/RHIIndexBuffer.h"

struct Vertex
{
    Vec3f position;
    Vec2f texCoord;
    Vec3f normal;
    Vec3f tangent;

    static RHIBindingDescription GetBindingDescription()
    {
        RHIBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = RHIVertexInputRate::VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<RHIVertexInputAttributeDescription, 4> GetAttributeDescriptions()
    {
        std::array<RHIVertexInputAttributeDescription, 4> attributeDescriptions{};

        // Position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = RHIFormat::R32G32B32_F;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        // Normal
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = RHIFormat::R32G32B32_F;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        // Texture coordinates
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = RHIFormat::R32G32_F;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
        
        // Tangent
        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = RHIFormat::R32G32B32_F;
        attributeDescriptions[3].offset = offsetof(Vertex, tangent);

        return attributeDescriptions;
    }
};

struct SubMesh
{
    uint32_t startIndex;
    uint32_t count;
};

class Mesh : public IResource
{
public:
    Mesh(std::filesystem::path path) : IResource(std::move(path)) {}
    Mesh(const Mesh&) = delete;
    Mesh(Mesh&&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    ~Mesh() override = default;

    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(RHIRenderer* renderer) override;
    void Unload() override;
    
    ResourceType GetResourceType() const override { return ResourceType::Mesh; }

    RHIVertexBuffer* GetVertexBuffer() const { return m_vertexBuffer.get(); }
    RHIIndexBuffer* GetIndexBuffer() const { return m_indexBuffer.get(); }
private:
    friend class Model;
    
    std::vector<float> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<SubMesh> m_subMeshes;

    std::unique_ptr<RHIVertexBuffer> m_vertexBuffer;
    std::unique_ptr<RHIIndexBuffer> m_indexBuffer;
};