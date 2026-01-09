#pragma once
#include <vector>

#include "IResource.h"
#include "Physic/BoundingBox.h"
#include "Utils/Type.h"

class Material;
class Scene;
class GameObject;
class Mesh;

class Model : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(Model)

    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(VulkanRenderer* renderer) override;
    void Unload() override;
    
    const std::vector<SafePtr<Mesh>>& GetMeshes() const { return m_meshes; }
    
    static SafePtr<GameObject> CreateGameObject(Model* model, Scene* scene);

private:
    void ComputeBoundingBox(const std::vector<std::vector<Vec3f>>& positionVertices);
private:
    std::vector<SafePtr<Mesh>> m_meshes;
    
    std::vector<SafePtr<Material>> m_materials = {};
    
    BoundingBox m_boundingBox;
};
