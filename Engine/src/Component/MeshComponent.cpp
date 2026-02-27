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
    auto lightManager = GetGameObject()->GetScene()->GetLightManager();

    for (auto& material : m_materials)
    {
        if (!material)
            continue;
        lightManager->SendLights(material.getPtr());
        material->SetAttribute("viewProj", VP);
    }
}

void MeshComponent::OnRender(VulkanRenderer* renderer) 
{
    if (!m_visible)
        return;
    
    auto queue = renderer->GetRenderQueueManager()->GetOpaqueQueue();
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

std::vector<SafePtr<Material>> MeshComponent::GetMaterials() const
{
    return m_materials;
}

