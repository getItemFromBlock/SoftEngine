#include "MeshComponent.h"

void MeshComponent::OnUpdate(float deltaTime)
{
    
}

void MeshComponent::OnRender()
{
}

void MeshComponent::SetMesh(const SafePtr<Mesh>& mesh)
{
    m_mesh = mesh;
}

void MeshComponent::AddMaterial(const SafePtr<Material>& material)
{
    m_materials.push_back(material);
}
