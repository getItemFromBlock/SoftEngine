#include "MeshComponent.h"

#include "Core/Engine.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Scene/GameObject.h"

#include "TransformComponent.h"
#include "Utils/Color.h"

void MeshComponent::Describe(ClassDescriptor& d)
{
    d.AddProperty("Mesh", PropertyType::Mesh, &m_mesh);
    d.AddProperty("Materials", PropertyType::Materials, &m_materials);
}

void MeshComponent::OnUpdate(float deltaTime)
{
    static float time;
    time += deltaTime;
    time = std::fmod(time, 360.f);
    
    RHIRenderer* renderer = Engine::Get()->GetRenderer();

    Mat4 VP = p_gameObject->GetScene()->GetCameraData().VP;

    for (auto& material : m_materials)
    {
        material->SetAttribute("viewProj", VP);
    }
}

void MeshComponent::OnRender(RHIRenderer* renderer) 
{
#ifdef RENDER_QUEUE
    auto queue = renderer->GetRenderQueueManager()->GetOpaqueQueue();
    queue->SubmitMeshRenderer(GetGameObject(), this->m_mesh.getPtr(), m_materials);
#else
    if (!m_mesh || !m_mesh->IsLoaded() || !m_mesh->SentToGPU() || !m_mesh->GetVertexBuffer() || !m_mesh->GetIndexBuffer())
        return;

    auto transformComponent = p_gameObject->GetComponent<TransformComponent>();
    auto model = transformComponent->GetWorldMatrix();
    // Render each submesh with its corresponding material
    size_t materialCount = m_materials.size();
        
    auto subMeshes = m_mesh->GetSubMeshes();
    for (size_t i = 0; i < subMeshes.size(); ++i)
    {
        size_t materialIndex = i % materialCount;
        auto& material = m_materials[materialIndex];
            
        auto shader = material->GetShader().getPtr();
        if (!renderer->BindShader(shader))
            continue;
        if (!renderer->BindMaterial(material.getPtr()))
            continue;

        renderer->BindVertexBuffers(m_mesh->GetVertexBuffer(), m_mesh->GetIndexBuffer());

        PushConstant pushConstant = shader->GetPushConstants()[ShaderType::Vertex];
        renderer->SendPushConstants(&model, sizeof(model), shader, pushConstant);
            
        renderer->DrawVertexSubMesh(m_mesh->GetIndexBuffer(), 
                                   subMeshes[i].startIndex, 
                                   subMeshes[i].count);
    }
#endif
}

void MeshComponent::SetMesh(const SafePtr<Mesh>& mesh)
{
    m_mesh = mesh;
}

void MeshComponent::AddMaterial(const SafePtr<Material>& material)
{
    m_materials.push_back(material);
}
