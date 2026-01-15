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

    m_shader->EOnSentToGPU.Bind([this, resourceManager]()
    {
        m_material = resourceManager->CreateMaterial(RESOURCE_PATH "/shaders/DebugDraw/debugDraw.mat");
        m_material->SetShader(m_shader.get());
    });

    m_initialized = true;
    return true;
}

void LineRenderer::Cleanup()
{
    if (m_vertexBuffer)
        m_vertexBuffer->Cleanup();
    if (m_indexBuffer)
        m_indexBuffer->Cleanup();
    
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
    m_vertices.clear();
    m_indices.clear();

    if (m_lines.empty())
    {
        m_needsRebuild = false;
        return;
    }

    for (const auto& line : m_lines)
    {
        uint32_t baseIndex = static_cast<uint32_t>(m_vertices.size());

        m_vertices.push_back({ line.start, line.color });
        m_vertices.push_back({ line.end,   line.color });

        m_indices.push_back(baseIndex);
        m_indices.push_back(baseIndex + 1);
    }

    constexpr uint32_t floatsPerVertex =
        (sizeof(Vec3f) + sizeof(Vec4f)) / sizeof(float);

    std::vector<float> packedVertices;
    packedVertices.reserve(m_vertices.size() * floatsPerVertex);

    for (const auto& v : m_vertices)
    {
        packedVertices.push_back(v.position.x);
        packedVertices.push_back(v.position.y);
        packedVertices.push_back(v.position.z);

        packedVertices.push_back(v.color.x);
        packedVertices.push_back(v.color.y);
        packedVertices.push_back(v.color.z);
        packedVertices.push_back(v.color.w);
    }

    m_vertexBuffer = m_renderer->CreateVertexBuffer(
        packedVertices.data(),
        static_cast<uint32_t>(packedVertices.size()),
        floatsPerVertex
    );

    m_indexBuffer = m_renderer->CreateIndexBuffer(
        m_indices.data(),
        static_cast<uint32_t>(m_indices.size())
    );

    m_needsRebuild = false;
}


void LineRenderer::UpdateCameraBuffer(VulkanRenderer* renderer, const Mat4& viewProj)
{
    m_material->SetAttribute("viewProj", viewProj);
}

void LineRenderer::Render(VulkanRenderer* renderer, const Mat4& viewProj)
{
    if (!m_initialized || m_lines.empty() || !m_material)
    {
        m_lines.clear();
        return;
    }

    if (m_needsRebuild)
        RebuildBuffers();

    if (!m_vertexBuffer || !m_indexBuffer)
    {
        m_lines.clear();
        return;
    }

    UpdateCameraBuffer(renderer, viewProj);
    
    if (!renderer->BindShader(m_shader.getPtr()))
    {
        m_lines.clear();
        return;
    }

    m_material->SendAllValues(renderer);
    if (!renderer->BindMaterial(m_material.getPtr()))
    {
        m_lines.clear();
        return;
    }

    renderer->BindVertexBuffers(m_vertexBuffer.get(), m_indexBuffer.get());
    
    renderer->DrawVertex(m_vertexBuffer.get(), m_indexBuffer.get());
    
    m_lines.clear();
}