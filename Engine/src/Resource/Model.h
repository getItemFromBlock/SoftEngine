#pragma once
#include <vector>

#include "IResource.h"
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
    bool SendToGPU(RHIRenderer* renderer) override;
    void Unload() override;
    
    const std::vector<SafePtr<Mesh>>& GetMeshes() const { return m_meshes; }
    
    static SafePtr<GameObject> CreateGameObject(Model* model, Scene* scene);

private:
    std::vector<SafePtr<Mesh>> m_meshes;
    
    std::vector<SafePtr<Material>> m_materials = {};
};
