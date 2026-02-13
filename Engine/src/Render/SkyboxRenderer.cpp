#include "SkyboxRenderer.h"

#include "Core/Engine.h"
#include "Resource/CubeMap.h"

SkyboxRenderer::SkyboxRenderer()
{
}

SkyboxRenderer::~SkyboxRenderer()
{
}

void SkyboxRenderer::RenderSkybox(VulkanRenderer* renderer, SafePtr<CubeMap> skybox, const Mat4& viewProjection) const
{
    if (!skybox.valid() || !m_material.valid() || !m_cubeMesh.valid())
        return;
    if (!skybox->SentToGPU() || !m_material->SentToGPU() || !m_cubeMesh->SentToGPU())
        return;
    m_material->SetAttribute("skybox", skybox);
    m_material->SetAttribute("viewProj", viewProjection);
    
    if (!renderer->BindShader(m_material->GetShader().getPtr()))
        return;
    if (!renderer->BindMaterial(m_material.getPtr()))
        return;
    m_material->SendAllValues(renderer);
    renderer->BindVertexBuffers(m_cubeMesh->GetVertexBuffer(), m_cubeMesh->GetIndexBuffer());
    uint32_t startIndex = m_cubeMesh->GetSubMeshes()[0].startIndex;
    uint32_t indexCount = m_cubeMesh->GetSubMeshes()[0].count;
    renderer->DrawVertexSubMesh(m_cubeMesh->GetIndexBuffer(), 
                                startIndex, 
                                indexCount);
}

void SkyboxRenderer::Initialize()
{
    ResourceManager* resourceManager = Engine::Get()->GetResourceManager();
    m_material = resourceManager->CreateMaterial("Skybox Material");
    SafePtr<Shader> shader = resourceManager->Load<Shader>(RESOURCE_PATH"shaders/Skybox/skybox.shader");
    m_cubeMesh = resourceManager->Load<Mesh>(RESOURCE_PATH"models/Skybox.obj/Skybox.mesh");
    m_material->SetShader(shader);
}
