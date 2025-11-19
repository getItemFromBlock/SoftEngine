#pragma once
#include <vector>

#include "IResource.h"

class Mesh;

class Model : public IResource
{
public:
    Model(const std::filesystem::path& path) : IResource(path) {}
    Model(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(const Model&) = delete;
    ~Model() override = default;

    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(RHIRenderer* renderer) override;
    void Unload() override;
    
    std::vector<Mesh*> GetMeshes() const { return m_meshes; }
private:
    std::vector<Mesh*> m_meshes;
};
