#include "MeshComponent.h"

#include "Core/Engine.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Scene/GameObject.h"

#include "TransformComponent.h"
#include "Utils/Color.h"

void MeshComponent::Describe(ClassDescriptor& d)
{
    d.AddProperty("Mesh", PropertyType::Mesh, &m_mesh);
    
    Property property;
    property.data = &m_materials;
    property.type = PropertyType::Material;
    property.name = "Materials";
    property.isList = true;
    d.AddProperty(property);
}

void MeshComponent::OnUpdate(float deltaTime)
{    
    if (!m_mesh)
        return;

    CameraData cameraData = p_gameObject->GetScene()->GetCameraData();
    auto transform = p_gameObject->GetTransform();
    m_visible = m_mesh->GetBoundingBox().IsOnFrustum(cameraData.frustum, transform.getPtr());
    if (!m_visible)
        return;
    Mat4 VP = cameraData.VP;

    for (auto& material : m_materials)
    {
        if (!material)
            continue;
        material->SetAttribute("cameraUBO.viewProj", VP);
    }
}

void MeshComponent::OnRender(VulkanRenderer* renderer) 
{
    if (!m_visible)
        return;
    
    auto queue = renderer->GetRenderQueueManager()->GetOpaqueQueue();
    if (!m_materials.empty() && m_materials[0]->GetShader().getPtr() != Engine::Get()->GetResourceManager()->GetDefaultShader().get())
    {
        queue = renderer->GetRenderQueueManager()->GetTransparentQueue(); // Use transparent queue to render in forward pass
    }
    queue->SubmitMeshRenderer(GetGameObject(), m_mesh.getPtr(), m_materials);
}

void MeshComponent::SetMesh(const SafePtr<Mesh>& mesh)
{
    m_mesh = mesh;
}

void MeshComponent::AddMaterial(const SafePtr<Material>& material)
{
    m_materials.push_back(material);
}

void MeshComponent::RemoveMaterial(const SafePtr<Material>& material)
{
    m_materials.erase(std::ranges::find_if(m_materials, [material](const SafePtr<Material>& mat)
    {
        return mat.getPtr() == material.getPtr();
    }));
}

void MeshComponent::SetMaterial(size_t index, const SafePtr<Material>& material)
{
    if (index >= m_materials.size())
    {
        m_materials.resize(index + 1);
    }
    m_materials[index] = material;
}

std::vector<SafePtr<Material>> MeshComponent::GetMaterials() const
{
    return m_materials;
}

SafePtr<Material> MeshComponent::GetMaterial(size_t index)
{
    if (index >= m_materials.size())
    {
        PrintWarning("index is bigger than material list size");
        return {};
    }
    return m_materials[index];
}

