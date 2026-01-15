#pragma once

#include "EngineAPI.h"
#include <vector>
#include <memory>

#include "Utils/Type.h"
#include <galaxymath/Maths.h>

class Material;
class VulkanRenderer;
class VulkanVertexBuffer;
class VulkanIndexBuffer;
class VulkanMaterial;
class Shader;

struct LineVertex
{
    Vec3f position;
    Vec4f color;
};

struct Line
{
    Vec3f start;
    Vec3f end;
    Vec4f color;
    float thickness;
};

class ENGINE_API LineRenderer
{
public:
    LineRenderer() = default;
    ~LineRenderer();

    bool Initialize(VulkanRenderer* renderer);
    void Cleanup();

    void AddLine(const Vec3f& start, const Vec3f& end, const Vec4f& color, float thickness = 1.0f);
    void Clear();

    void Render(VulkanRenderer* renderer, const Mat4& viewProj);

private:
    void RebuildBuffers();
    void UpdateCameraBuffer(VulkanRenderer* renderer, const Mat4& viewProj);

private:
    VulkanRenderer* m_renderer = nullptr;
    
    std::vector<Line> m_lines;
    std::vector<LineVertex> m_vertices;
    std::vector<uint32_t> m_indices;
    
    std::unique_ptr<VulkanVertexBuffer> m_vertexBuffer;
    std::unique_ptr<VulkanIndexBuffer> m_indexBuffer;
    SafePtr<Material> m_material;
    
    SafePtr<Shader> m_shader;
    
    bool m_needsRebuild = false;
    bool m_initialized = false;
};