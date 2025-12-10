#include "MeshComponent.h"

#include "Core/Engine.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Scene/GameObject.h"

#include "TransformComponent.h"

void MeshComponent::OnUpdate(float deltaTime)
{
    RHIRenderer* renderer = Engine::Get()->GetRenderer();

    Mat4 VP = p_gameObject->GetScene()->GetCameraData().VP;

    for (auto& material : m_materials)
    {
        {
            Uniform uniform = material->GetShader()->GetUniform("ubo");
            auto binding = UBOBinding(uniform.set, uniform.binding);
            material->GetShader()->SendValue(binding, &VP, sizeof(VP), renderer);
        }
        {
            Vec4f color = Vec4f::One();
            Uniform uniform = material->GetShader()->GetUniform("material");
            auto binding = UBOBinding(uniform.set, uniform.binding);
            material->GetShader()->SendValue(binding, &color, sizeof(color), renderer);
        }
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
