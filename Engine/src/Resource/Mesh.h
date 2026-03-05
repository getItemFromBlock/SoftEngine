#pragma once
#include <vector>
#include <memory>
#include <array>

#include <galaxymath/Maths.h>

#include "IResource.h"
#include "Physic/BoundingBox.h"

#include "Render/Vulkan/VulkanVertexBuffer.h"
#include "Render/Vulkan/VulkanIndexBuffer.h"

class VulkanBuffer;

struct Vertex
{
    Vec3f position;
    Vec2f texCoord;
    Vec3f normal;
    Vec4f tangent;
};

struct WeightedVertex
{
    Vec3f position;
    Vec2f texCoord;
    Vec3f normal;
    Vec4f tangent;
    Vec4i indices;
    Vec4f weights;
};

struct SubMesh
{
    uint32_t startIndex;
    uint32_t count;
};

class Mesh : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(Mesh)

    bool Load(ResourceManager *resourceManager) override;
    bool SendToGPU(VulkanRenderer *renderer) override;
    // verticeData is a pointer to a list of Vertex or WeightedVertex structs, with no padding in-between
    void CreateFrom(float *verticeData, uint32_t verticeCount, uint32_t *indices, uint32_t indiceCount, bool isWeighted = false);
    void Unload() override;
    
    bool Exists() const override;

    VulkanVertexBuffer* GetVertexBuffer() const { return m_vertexBuffer.get(); }
    VulkanIndexBuffer* GetIndexBuffer() const { return m_indexBuffer.get(); }
    
    const std::vector<SubMesh>& GetSubMeshes() const { return m_subMeshes; }
    
    const BoundingBox& GetBoundingBox() const { return m_boundingBox; }

private:
    void ComputeBoundingBox(const std::vector<Vec3f>& positionVertices);
private:
    friend class Model;
    
    std::vector<float> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<SubMesh> m_subMeshes;

    std::unique_ptr<VulkanVertexBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanIndexBuffer> m_indexBuffer;
    
    BoundingBox m_boundingBox;
    bool m_isWeighted = false;
};