#pragma once

#include "IResource.h"
#include "Loader/GLTFLoader.h"

#include <vector>

#include "Physic/BoundingBox.h"

class Scene;
class GameObject;

class Model : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(Model)

    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(VulkanRenderer* renderer) override;
    void Unload() override;
    
    const std::vector<SafePtr<Mesh>>& GetMeshes() const { return m_meshes; }
    
    static SafePtr<GameObject> CreateGameObject(Model* model, Scene* scene, GameObject* parent = nullptr);

private:
    void ComputeBoundingBox(const std::vector<std::vector<Vec3f>>& positionVertices);

private:
    friend class FBXLoader;
    
    std::vector<SafePtr<Mesh>>     m_meshes;
    std::vector<SafePtr<Material>> m_materials;
    BoundingBox                    m_boundingBox;

    // GLTF-only: node hierarchy and per-mesh owning node, populated by GLTFLoader.
    std::vector<GLTFLoader::Node>  m_gltfNodes;
    std::vector<int>               m_gltfRootNodes;
    std::vector<int>               m_meshNodeIndices;
};
