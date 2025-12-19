#pragma once
#include "IComponent.h"
#include "Resource/Mesh.h"
#include "Utils/Type.h"

class Material;

class MeshComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(MeshComponent)
    
    void Describe(ClassDescriptor& d) override;
    
    void OnUpdate(float deltaTime) override;
    void OnRender(RHIRenderer* renderer) override;
    
    void SetMesh(const SafePtr<Mesh>& mesh);
    
    void AddMaterial(const SafePtr<Material>& material);
private:
    std::vector<SafePtr<Material>> m_materials;
    SafePtr<Mesh> m_mesh;
    bool m_visible = true;
};
