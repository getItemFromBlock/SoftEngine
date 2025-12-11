#include "MeshComponent.h"

#include "Core/Engine.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Scene/GameObject.h"

#include "TransformComponent.h"
#include "Utils/Color.h"

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
        
        material->SetAttribute("color", static_cast<Vec4f>(Color::FromHSV(time, 1.f, 1.f)));
        
        material->SendAllValues(renderer);
    }
}

void MeshComponent::OnRender(RHIRenderer* renderer)
{
    if (!m_mesh || !m_mesh->IsLoaded() || !m_mesh->SentToGPU() || !m_mesh->GetVertexBuffer() || !m_mesh->GetIndexBuffer())
        return;

    auto transformComponent = p_gameObject->GetComponent<TransformComponent>();
    auto model = transformComponent->GetModelMatrix();
    for (auto& material : m_materials)
    {
        auto shader = material->GetShader().get().get();
        renderer->BindShader(shader);

        renderer->BindVertexBuffers(m_mesh->GetVertexBuffer(), m_mesh->GetIndexBuffer());

        PushConstant pushConstant = shader->GetPushConstants()[ShaderType::Vertex];
        renderer->SendPushConstants(&model, sizeof(model), shader, pushConstant);
        
        renderer->DrawVertex(m_mesh->GetVertexBuffer(), m_mesh->GetIndexBuffer());
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
