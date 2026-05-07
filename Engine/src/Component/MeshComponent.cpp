#include "MeshComponent.h"

#include "Core/Engine.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Scene/GameObject.h"

#include "TransformComponent.h"
#include "Scene/SceneSerializer.h"
#include "Utils/Color.h"

void MeshComponent::Describe(ClassDescriptor& d)
{
    d.AddProperty("Mesh", PropertyType::Mesh, &m_mesh);
    
    Property property;
    property.data = &m_materials;
    property.type = PropertyType::Material;
    property.name = "Materials";
    property.isList = true;
    property.addElement = [this]()
    {
        auto resourceManager = Engine::Get()->GetResourceManager();
        AddMaterial(resourceManager->GetDefaultMaterial());  
    };
    property.removeElement = [this](int index)
    {
        auto material = m_materials[index];
        RemoveMaterial(material);
    };
    property.setElement = [this](size_t index, void* materialData)
    {
        SafePtr<Material> material = *static_cast<SafePtr<Material>*>(materialData);
        SetMaterial(index, material);
    };
    
    d.AddProperty(property);
    d.AddBool("Draw Bounds", m_drawBounds);
}

void MeshComponent::OnUpdate(float deltaTime)
{    
    if (!m_mesh)
        return;

    CameraData cameraData = p_gameObject->GetScene()->GetCameraData();
    auto transform = p_gameObject->GetTransform();
    m_visible = m_mesh->GetBoundingBox().IsOnFrustum(cameraData.frustum, transform.getPtr());
}

void MeshComponent::OnGameUpdate(float deltaTime)
{
    OnUpdate(deltaTime);
}

void MeshComponent::OnRender(VulkanRenderer* renderer) 
{
    if (!m_mesh || !m_visible)
        return;
    
    CameraData cameraData = p_gameObject->GetScene()->GetCameraData();
    Mat4 VP = cameraData.VP;

    for (auto& material : m_materials)
    {
        if (!material)
            continue;
        material->SetAttribute("cameraUBO.viewProj", VP);
    }
    
    auto queue = renderer->GetRenderQueueManager()->GetOpaqueQueue();
    if (!m_materials.empty() && m_materials[0] && m_materials[0]->GetShader().getPtr() != Engine::Get()->GetResourceManager()->GetDefaultShader().get())
    {
        queue = renderer->GetRenderQueueManager()->GetTransparentQueue(); // Use transparent queue to render in forward pass
    }
    queue->SubmitMeshRenderer(GetGameObject(), m_mesh.getPtr(), m_materials);
    
    if (m_drawBounds)
    {
        BoundingBox bounds = m_mesh->GetBoundingBox();
        renderer->DrawWireCube(bounds.GetCenter(), bounds.GetExtents(), Vec4f(1.f, 0.f, 0.f, 1.f));
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

nlohmann::json MeshComponent::Serialize() const
{
    nlohmann::json materials = nlohmann::json::array();
    for (const auto& material : GetMaterials())
        materials.push_back(SceneSerializer::SerializeMaterial(material));

    return {
        {"mesh", SceneSerializer::SerializeResourcePath(GetMesh())},
        {"materials", materials},
        {"drawBounds", GetDrawBounds()}
    };
}

void MeshComponent::Deserialize(const nlohmann::json& json)
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    SetMesh(SceneSerializer::LoadResource<Mesh>(resourceManager, json.contains("mesh") ? json["mesh"] : nlohmann::json()));
    SetDrawBounds(json.value("drawBounds", GetDrawBounds()));

    if (json.contains("materials") && json["materials"].is_array())
    {
        size_t materialIndex = 0;
        for (const nlohmann::json& materialData : json["materials"])
        {
            SetMaterial(materialIndex, SceneSerializer::DeserializeMaterial(materialData, resourceManager));
            ++materialIndex;
        }
    }
}

