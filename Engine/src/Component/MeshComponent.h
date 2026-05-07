#pragma once
#include "IComponent.h"
#include "Resource/Mesh.h"
#include "Utils/Type.h"

class Material;
class SceneSerializer;

class MeshComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(MeshComponent)

    void Describe(ClassDescriptor& d) override;

    void OnUpdate(float deltaTime) override;
    void OnGameUpdate(float deltaTime) override;
    void OnRender(VulkanRenderer* renderer) override;

    void SetMesh(const SafePtr<Mesh>& mesh);

    void AddMaterial(const SafePtr<Material>& material);
    void RemoveMaterial(const SafePtr<Material>& material);
    void SetMaterial(size_t index, const SafePtr<Material>& material);

    SafePtr<Mesh> GetMesh() const { return m_mesh; }
    std::vector<SafePtr<Material>> GetMaterials() const;
    SafePtr<Material> GetMaterial(size_t index);
    bool GetDrawBounds() const { return m_drawBounds; }
    void SetDrawBounds(bool drawBounds) { m_drawBounds = drawBounds; }

    nlohmann::json Serialize() const override;
    void Deserialize(const nlohmann::json& json) override;
private:
    friend class SceneSerializer;

    std::vector<SafePtr<Material>> m_materials;
    SafePtr<Mesh> m_mesh;
    bool m_visible = true;
    bool m_drawBounds = false;
};
