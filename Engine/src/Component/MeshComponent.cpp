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
        
        material->SendAllValues(renderer);
    }
}

void MeshComponent::OnRender(RHIRenderer* renderer) 
{
    /*
    if (!m_mesh || !m_mesh->IsLoaded()) return;

    auto transform = p_gameObject->GetComponent<TransformComponent>()->GetWorldMatrix();
    auto subMeshes = m_mesh->GetSubMeshes();

    for (size_t i = 0; i < subMeshes.size(); ++i) 
    {
        DrawCommandData data;
        data.mesh = m_mesh.getPtr();
        data.subMeshIndex = i;
        data.material = m_materials[i % m_materials.size()].getPtr();
        data.transform = transform;

        // Generate Sort Key: 
        // [ 8-bit Layer | 24-bit Pipeline ID | 32-bit Material ID ]
        uint64_t pipelineID = data.material->GetShader()->GetID();
        uint64_t materialID = data.material->GetID();
        
        data.sortKey = (pipelineID << 32) | materialID;

        renderer->Submit(item);
    }
    */
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
        if (!renderer->BindMaterial(material.getPtr()))
            continue;

        renderer->BindVertexBuffers(m_mesh->GetVertexBuffer(), m_mesh->GetIndexBuffer());

        PushConstant pushConstant = shader->GetPushConstants()[ShaderType::Vertex];
        renderer->SendPushConstants(&model, sizeof(model), shader, pushConstant);
            
        renderer->DrawVertexSubMesh(m_mesh->GetIndexBuffer(), 
                                   subMeshes[i].startIndex, 
                                   subMeshes[i].count);
    }
}

void MeshComponent::SetMesh(const SafePtr<Mesh>& mesh)
{
    m_mesh = mesh;
}

void MeshComponent::AddMaterial(const SafePtr<Material>& material)
{
    m_materials.push_back(material);
}
