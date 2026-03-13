#include "SkyboxRenderer.h"

#include "Core/Engine.h"
#include "Resource/CubeMap.h"

SkyboxRenderer::SkyboxRenderer()
{
}

SkyboxRenderer::~SkyboxRenderer()
{
}

void SkyboxRenderer::RenderSkybox(VulkanRenderer* renderer, const SafePtr<Material>& material, const Mat4& viewProjection) const
{
    if (!material.valid() || !m_cubeMesh.valid())
        return;
    if (!material->HasBeenSent() || !m_cubeMesh->HasBeenSent())
        return;
    material->SetAttribute("cameraUBO.viewProj", viewProjection);
    
    if (!renderer->BindShader(material->GetShader().getPtr()))
        return;
    if (!renderer->BindMaterial(material.getPtr()))
        return;
    material->SendAllValues(renderer);
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
    SafePtr<Shader> shader = resourceManager->Load<Shader>(RESOURCE_PATH"shaders/Skybox/skybox.shader");
    m_cubeMesh = resourceManager->Load<Mesh>(RESOURCE_PATH"models/Skybox.obj/Skybox.mesh");
}
