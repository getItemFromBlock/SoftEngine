#include "LineRenderer.h"

#include "Core/Engine.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Render/Vulkan/VulkanVertexBuffer.h"
#include "Render/Vulkan/VulkanIndexBuffer.h"
#include "Render/Vulkan/VulkanMaterial.h"
#include "Resource/Shader.h"

LineRenderer::~LineRenderer()
{
    Cleanup();
}

bool LineRenderer::Initialize(VulkanRenderer* renderer)
{
    if (!renderer)
        return false;

    m_renderer = renderer;
    
    auto resourceManager = Engine::Get()->GetResourceManager();
    m_shader = resourceManager->Load<Shader>(RESOURCE_PATH "/shaders/DebugDraw/debugDraw.shader");

    m_shader->EOnSentToGPU.Bind([&]()
    {
        m_material = renderer->CreateMaterial(m_shader.getPtr());
    });

    m_initialized = true;
    return true;
}

void LineRenderer::Cleanup()
{
    m_vertexBuffer.reset();
    m_indexBuffer.reset();
    m_material.reset();
    
    m_lines.clear();
    m_vertices.clear();
    m_indices.clear();
    
    m_initialized = false;
}

void LineRenderer::AddLine(const Vec3f& start, const Vec3f& end, const Vec4f& color, float thickness)
{
    m_lines.push_back({ start, end, color, thickness });
    m_needsRebuild = true;
}

void LineRenderer::Clear()
{
    m_lines.clear();
    m_vertices.clear();
    m_indices.clear();
    m_needsRebuild = true;
}

void LineRenderer::RebuildBuffers()
{
    if (m_lines.empty())
        return;

    m_vertices.clear();
    m_indices.clear();

    for (const auto& line : m_lines)
    {
        uint32_t baseIndex = static_cast<uint32_t>(m_vertices.size());

        m_vertices.push_back({ line.start, line.color });
        m_vertices.push_back({ line.end, line.color });

        m_indices.push_back(baseIndex);
        m_indices.push_back(baseIndex + 1);
    }

    if (!m_vertices.empty())
    {
        const float* vertexData = reinterpret_cast<const float*>(m_vertices.data());
        uint32_t vertexDataSize = static_cast<uint32_t>(m_vertices.size() * sizeof(LineVertex));
        uint32_t floatsPerVertex = sizeof(LineVertex) / sizeof(float);
        
        m_vertexBuffer = m_renderer->CreateVertexBuffer(vertexData, vertexDataSize, floatsPerVertex);
    }

    if (!m_indices.empty())
    {
        uint32_t indexDataSize = static_cast<uint32_t>(m_indices.size() * sizeof(uint32_t));
        m_indexBuffer = m_renderer->CreateIndexBuffer(m_indices.data(), indexDataSize);
    }

    m_needsRebuild = false;
}

void LineRenderer::UpdateCameraBuffer(VulkanRenderer* renderer, const Mat4& viewProj)
{
    m_material->SetUniformData(0, 0, &viewProj, sizeof(Mat4), renderer);
}

void LineRenderer::Render(VulkanRenderer* renderer, const Mat4& viewProj)
{
    if (!m_initialized || m_lines.empty() || !m_material)
        return;

    if (m_needsRebuild)
        RebuildBuffers();

    if (!m_vertexBuffer || !m_indexBuffer)
        return;

    UpdateCameraBuffer(renderer, viewProj);

    m_material->Bind(renderer);

    Mat4 identityMatrix = Mat4::Identity();
    PushConstant pushConstant = {}; 
    renderer->SendPushConstants(&identityMatrix, sizeof(Mat4), m_shader.getPtr(), pushConstant);

    renderer->BindVertexBuffers(m_vertexBuffer.get(), m_indexBuffer.get());
    
    renderer->DrawVertex(m_vertexBuffer.get(), m_indexBuffer.get());
}