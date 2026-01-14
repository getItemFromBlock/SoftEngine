#include "Model.h"

#include "Mesh.h"
#include "ResourceManager.h"

#include "Component/MeshComponent.h"
#include "Core/Engine.h"

#include "Loader/OBJLoader.h"

#include "Debug/Log.h"

#include "Scene/GameObject.h"
#include "Scene/Scene.h"

#include "Utils/Color.h"
#include "Utils/File.h"

#include "Resource/Texture.h"

bool Model::Load(ResourceManager* resourceManager)
{
    if (p_path.extension() == ".obj")
    {
        OBJLoader::Model model;
        if (!OBJLoader::Load(p_path, model))
        {
            PrintError("Failed to load model %s", p_path.filename().generic_string().c_str());
            return false;
        }
        
        std::unordered_map<std::string, SafePtr<Material>> materials;
        for (auto& mat : model.materials)
        {
            std::filesystem::path matPath = p_path / mat.name;
            if (File::Exist(matPath))
            {
                resourceManager->Load<Material>(matPath);
            }
            else
            {
                SafePtr<Material> matResource = resourceManager->CreateMaterial(matPath);
                
                if (mat.albedo.has_value())
                {
                    auto texture = resourceManager->Load<Texture>(p_path.parent_path() / mat.albedo.value());
                    matResource->SetAttribute("albedoSampler", texture);
                }
                matResource->SetAttribute("color", static_cast<Vec4f>(Color(mat.diffuse, mat.transparency)));
                
                materials[mat.name.generic_string()] = matResource;
            }
        }
        
        std::vector<std::vector<Vec3f>> positions;
        positions.reserve(model.meshes.size());
        for (size_t i = 0; i < model.meshes.size(); i++)
        {
            auto& mesh = model.meshes[i];
            positions.push_back(mesh.positions);
            std::filesystem::path meshPath = p_path / (mesh.name.generic_string() + ".mesh");
            SafePtr meshResource = resourceManager->GetResource<Mesh>(meshPath);
            if (!meshResource)
            {
                meshResource = resourceManager->AddResource(
                    std::make_shared<Mesh>(meshPath)
                );
            }
            
            meshResource->m_subMeshes.reserve(mesh.subMeshes.size());
            for (const auto& subMesh : mesh.subMeshes)
            {
                meshResource->m_subMeshes.push_back(SubMesh(subMesh.startIndex, subMesh.count));
                if (subMesh.materialName.has_value())
                    m_materials.push_back(materials[subMesh.materialName.value()]);
            }
            
            meshResource->ComputeBoundingBox(mesh.positions);
            
            m_boundingBox.min.x = std::min(m_boundingBox.min.x, meshResource->GetBoundingBox().min.x);
            m_boundingBox.min.y = std::min(m_boundingBox.min.y, meshResource->GetBoundingBox().min.y);
            m_boundingBox.min.z = std::min(m_boundingBox.min.z, meshResource->GetBoundingBox().min.z);

            m_boundingBox.max.x = std::max(m_boundingBox.max.x, meshResource->GetBoundingBox().max.x);
            m_boundingBox.max.y = std::max(m_boundingBox.max.y, meshResource->GetBoundingBox().max.y);
            m_boundingBox.max.z = std::max(m_boundingBox.max.z, meshResource->GetBoundingBox().max.z);
            
            m_meshes.push_back(meshResource);
            
            meshResource->m_vertices = mesh.finalVertices;
            for (const Vec3i& idx : mesh.indices)
            {                
                meshResource->m_indices.push_back(idx.x);
                meshResource->m_indices.push_back(idx.y);
                meshResource->m_indices.push_back(idx.z);
            }
            meshResource->SetLoaded();
            ASSERT(!meshResource->m_vertices.empty())
            resourceManager->AddResourceToSend(meshResource.getPtr());
        }
        return true;
    }
    else
    {
        PrintError("Unsupported file format %s", p_path.extension().generic_string().c_str());
        return false;
    }
}

bool Model::SendToGPU(VulkanRenderer* renderer)
{
    UNUSED(renderer);
    return true;
}

void Model::Unload()
{
}

SafePtr<GameObject> Model::CreateGameObject(Model* model, Scene* scene)
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    SafePtr<GameObject> go = scene->CreateGameObject();
    go->SetName(model->GetName());
	size_t materialIndex = 0;
    for (size_t i = 0; i < model->m_meshes.size(); i++)
    {
        SafePtr<GameObject> child = scene->CreateGameObject(go.getPtr());
        child->SetName(model->m_meshes[i]->GetName());
        SafePtr<MeshComponent> meshComp = child->AddComponent<MeshComponent>();
        auto subMeshes = model->m_meshes[i]->GetSubMeshes();
        for (size_t j = 0; j < subMeshes.size(); j++)
        {
            if (materialIndex >= model->m_materials.size())
            {
                meshComp->AddMaterial(resourceManager->GetDefaultMaterial());
                continue;
            }
            meshComp->AddMaterial(model->m_materials[materialIndex++].get());
        }
        meshComp->SetMesh(model->m_meshes[i]);
    }
    return go;
}

void Model::ComputeBoundingBox(const std::vector<std::vector<Vec3f>>& positionVertices)
{
    if (positionVertices.empty())
        return;
    for (size_t i = 0; i < m_meshes.size(); i++)
    {
        SafePtr<Mesh>& mesh = m_meshes[i];
        if (!mesh)
            continue;
        mesh->ComputeBoundingBox(positionVertices[i]);

        m_boundingBox.min.x = std::min(m_boundingBox.min.x, mesh->m_boundingBox.min.x);
        m_boundingBox.min.y = std::min(m_boundingBox.min.y, mesh->m_boundingBox.min.y);
        m_boundingBox.min.z = std::min(m_boundingBox.min.z, mesh->m_boundingBox.min.z);

        m_boundingBox.max.x = std::max(m_boundingBox.max.x, mesh->m_boundingBox.max.x);
        m_boundingBox.max.y = std::max(m_boundingBox.max.y, mesh->m_boundingBox.max.y);
        m_boundingBox.max.z = std::max(m_boundingBox.max.z, mesh->m_boundingBox.max.z);
    }
}
