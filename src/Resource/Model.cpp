#include "Model.h"

#include "Mesh.h"
#include "ResourceManager.h"
#include "Loader/OBJLoader.h"

bool Model::Load(ResourceManager* resourceManager)
{
    if (p_path.extension() == ".obj")
    {
        OBJLoader::Model model;
        if (!OBJLoader::Load(p_path, model))
        {
            throw std::runtime_error("Failed to load model");
            return false;
        }
        
        for (auto& mesh : model.meshes)
        {
            SafePtr<Mesh> meshResource = resourceManager->AddResource(std::make_shared<Mesh>(p_path / mesh.name));
            
            m_meshes.push_back(meshResource.get().get());
            
            meshResource->m_vertices = mesh.finalVertices;
            for (const Vec3i& idx : mesh.indices)
            {                
                meshResource->m_indices.push_back(idx.x);
                meshResource->m_indices.push_back(idx.y);
                meshResource->m_indices.push_back(idx.z);
            }
            meshResource->SetLoaded();
            resourceManager->AddResourceToSend(meshResource.get().get());
        } 
        return true;
    }
    else
    {
        throw std::runtime_error("Unsupported file format" + p_path.extension().generic_string());
        return false;
    }
}

bool Model::SendToGPU(RHIRenderer* renderer)
{
    return true;
}

void Model::Unload()
{
}
