#include "MeshComponent.h"

#include "Core/Engine.h"
#include "Render/Vulkan/VulkanRenderer.h"

void MeshComponent::OnUpdate(float deltaTime)
{
    RHIRenderer* renderer = Engine::Get()->GetRenderer();
    Vec2i windowSize = Engine::Get()->GetWindow()->GetSize();

    for (auto& material : m_materials)
    {
        UniformBufferObject ubo;
        Vec3f camPos = Vec3f(2.0f, 2.0f, 2.0f);
        Vec3f camTarget = Vec3f(0.0f, 0.0f, 0.0f);
        Vec3f camUp = Vec3f(0.0f, 1.0f, 0.0f);

        float distanceInFront = 5.f;
        Vec3f forward = Vec3f::Normalize(camTarget - camPos);
        Vec3f cubePosition = camPos + forward * distanceInFront;

        float angle = 90.f;
        ubo.Model = Mat4::CreateTransformMatrix(cubePosition, Vec3f(0.f, angle, 0.f), Vec3f(1.f, 1.f, 1.f));

        ubo.View = Mat4::LookAtRH(camPos, camTarget, camUp);

        ubo.Projection = Mat4::CreateProjectionMatrix(
            45.f, (float)windowSize.x / (float)windowSize.y, 0.1f, 10.0f);
        ubo.Projection[1][1] *= -1; // GLM -> Vulkan Y flip

        material->GetShader()->SendValue(&ubo, sizeof(ubo), renderer);
        // material->GetShader()->get
    }
}

void MeshComponent::OnRender(RHIRenderer* renderer)
{
    if (!m_mesh || !m_mesh->IsLoaded() || !m_mesh->SentToGPU() || !m_mesh->GetVertexBuffer() || !m_mesh->GetIndexBuffer())
        return;
    for (auto& material : m_materials)
    {
        renderer->BindShader(material->GetShader().get().get());

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
