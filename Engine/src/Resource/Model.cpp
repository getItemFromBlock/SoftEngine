#include "Model.h"

#include "Mesh.h"
#include "ResourceManager.h"

#include "Component/MeshComponent.h"
#include "Core/Engine.h"

#include "Loader/OBJLoader.h"

#include "Debug/Log.h"
#include "Loader/FBXLoader.h"
#include "Loader/GLTFLoader.h"

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
            std::filesystem::path matPath = p_path / (mat.name.generic_string() + ".mat");

            SafePtr<Material> matResource = {};
            if (resourceManager->GetResource<Material>(matPath))
            {
                matResource = resourceManager->GetResource<Material>(matPath);
                matResource->SetShader(resourceManager->GetDefaultShader());
            }
            else
            {
                matResource = resourceManager->CreateMaterial(matPath);
            }

            if (mat.albedo.has_value())
            {
                SafePtr<Texture> texture = resourceManager->Load<
                    Texture>(p_path.parent_path() / mat.albedo.value());
                matResource->SetAttribute("albedoSampler", texture);
            }
            SafePtr<Texture> normal;
            SafePtr<Texture> metallic;
            SafePtr<Texture> roughness;
            TextureParam param;
            param.format = TextureFormat::UNORM;
            if (mat.normal.has_value())
            {
                normal = resourceManager->Load<Texture>(p_path.parent_path() / mat.normal.value());
                normal->SetTextureParameters(param);
            }
            else
            {
                normal = resourceManager->GetDefaultNormal();
            }

            if (mat.metallic.has_value())
            {
                metallic = resourceManager->Load<Texture>(p_path.parent_path() / mat.metallic.value());
                matResource->SetAttribute("material.metalnessFactor", 1.f);
            }
            else
            {
                metallic = resourceManager->Load<Texture>(RESOURCE_PATH"textures/black.png");
                matResource->SetAttribute("material.metalnessFactor", 0.f);
            }
            metallic->SetTextureParameters(param);

            if (mat.roughness.has_value())
            {
                roughness = resourceManager->Load<Texture>(p_path.parent_path() / mat.roughness.value());
                matResource->SetAttribute("material.roughnessFactor", 1.f);
            }
            else
            {
                roughness = resourceManager->Load<Texture>(RESOURCE_PATH"textures/black.png");
                matResource->SetAttribute("material.roughnessFactor", 0.f);
            }

            roughness->SetTextureParameters(param);
            matResource->SetAttribute("normalSampler", normal);
            matResource->SetAttribute("metalnessSampler", metallic);
            matResource->SetAttribute("roughnessSampler", roughness);
            matResource->SetAttribute("aoSampler",
                                      resourceManager->Load<Texture>(RESOURCE_PATH"textures/black.png"));
            matResource->SetAttribute("material.color", static_cast<Vec4f>(Color(mat.diffuse, mat.transparency)));

            materials[mat.name.generic_string()] = matResource;
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
            meshResource->SetLoaded();
            ASSERT(!meshResource->m_vertices.empty())
            resourceManager->AddResourceToSend(meshResource.getPtr());
        }
        return true;
    }
    else if (p_path.extension() == ".gltf")
    {
        GLTFLoader::Model model;
        if (!GLTFLoader::Load(p_path, model))
        {
            PrintError("Failed to load model %s", p_path.filename().generic_string().c_str());
            return false;
        }

        // Materials
        std::unordered_map<std::string, SafePtr<Material>> materials;
        size_t index = 0;
        for (auto& mat : model.materials)
        {
            std::string matName = mat.name.generic_string();
            ASSERT(!matName.empty())
            
            //TODO: Fix with OBJ
            std::filesystem::path matPath = p_path / (matName + ".mat");

            SafePtr<Material> matResource = {};
            if (resourceManager->GetResource<Material>(matPath))
            {
                matResource = resourceManager->GetResource<Material>(matPath);
                matResource->SetShader(resourceManager->GetDefaultShader());
            }
            else
            {
                matResource = resourceManager->CreateMaterial(matPath);
            }


            TextureParam linearParam;
            linearParam.format = TextureFormat::UNORM;

            if (mat.albedo.has_value())
            {
                SafePtr<Texture> texture = resourceManager->Load<Texture>(
                    p_path.parent_path() / mat.albedo.value());
                matResource->SetAttribute("albedoSampler", texture);
            }

            matResource->SetAttribute("material.color", mat.color);

            SafePtr<Texture> normal;
            if (mat.normal.has_value())
            {
                normal = resourceManager->Load<Texture>(p_path.parent_path() / mat.normal.value());
                normal->SetTextureParameters(linearParam);
            }
            else
            {
                normal = resourceManager->GetDefaultNormal();
            }
            matResource->SetAttribute("normalSampler", normal);

            SafePtr<Texture> metallic;
            if (mat.metallic.has_value())
            {
                metallic = resourceManager->Load<Texture>(p_path.parent_path() / mat.metallic.value());
                matResource->SetAttribute("material.metalnessFactor", mat.metallicFactor);
            }
            else
            {
                metallic = resourceManager->Load<Texture>(RESOURCE_PATH"textures/black.png");
                matResource->SetAttribute("material.metalnessFactor", 0.f);
            }
            metallic->SetTextureParameters(linearParam);
            matResource->SetAttribute("metalnessSampler", metallic);

            SafePtr<Texture> roughness;
            if (mat.roughness.has_value())
            {
                roughness = resourceManager->Load<Texture>(p_path.parent_path() / mat.roughness.value());
                matResource->SetAttribute("material.roughnessFactor", mat.roughnessFactor);
            }
            else
            {
                roughness = resourceManager->Load<Texture>(RESOURCE_PATH"textures/black.png");
                matResource->SetAttribute("material.roughnessFactor", 0.f);
            }
            roughness->SetTextureParameters(linearParam);
            matResource->SetAttribute("roughnessSampler", roughness);
            
            SafePtr<Texture> ao;
            if (mat.ao.has_value())
            {
                ao = resourceManager->Load<Texture>(p_path.parent_path() / mat.ao.value());
                ao->SetTextureParameters(linearParam);
                matResource->SetAttribute("material.aoFactor", 1.f);
            }
            else
            {
                matResource->SetAttribute("material.roughnessFactor", 0.f);
            }
            matResource->SetAttribute("aoSampler", ao);

            materials[matName] = matResource;
            index++;
        }

        // Meshes 
        for (size_t i = 0; i < model.meshes.size(); i++)
        {
            auto& mesh = model.meshes[i];

            std::filesystem::path meshPath = p_path / (mesh.name.generic_string() + ".mesh");
            SafePtr<Mesh> meshResource = resourceManager->GetResource<Mesh>(meshPath);
            if (!meshResource)
            {
                meshResource = resourceManager->AddResource(
                    std::make_shared<Mesh>(meshPath));
            }

            meshResource->m_subMeshes.reserve(mesh.subMeshes.size());
            for (const auto& subMesh : mesh.subMeshes)
            {
                meshResource->m_subMeshes.push_back(SubMesh(subMesh.startIndex, subMesh.count));
                if (subMesh.materialName.has_value())
                {
                    const std::string& name = subMesh.materialName.value();
                    auto it = materials.find(name);
                    m_materials.push_back(it != materials.end()
                                              ? it->second
                                              : resourceManager->GetDefaultMaterial());
                }
            }

            meshResource->ComputeBoundingBox(mesh.positions);

            m_boundingBox.min.x = std::min(m_boundingBox.min.x, meshResource->GetBoundingBox().min.x);
            m_boundingBox.min.y = std::min(m_boundingBox.min.y, meshResource->GetBoundingBox().min.y);
            m_boundingBox.min.z = std::min(m_boundingBox.min.z, meshResource->GetBoundingBox().min.z);

            m_boundingBox.max.x = std::max(m_boundingBox.max.x, meshResource->GetBoundingBox().max.x);
            m_boundingBox.max.y = std::max(m_boundingBox.max.y, meshResource->GetBoundingBox().max.y);
            m_boundingBox.max.z = std::max(m_boundingBox.max.z, meshResource->GetBoundingBox().max.z);

            m_meshes.push_back(meshResource);

            m_meshNodeIndices.push_back(-1);
            for (size_t ni = 0; ni < model.nodes.size(); ++ni)
            {
                if (model.nodes[ni].meshIndex == static_cast<int>(i))
                {
                    m_meshNodeIndices.back() = static_cast<int>(ni);
                    break;
                }
            }

            meshResource->m_vertices = mesh.finalVertices;
            meshResource->SetLoaded();
            ASSERT(!meshResource->m_vertices.empty())
            resourceManager->AddResourceToSend(meshResource.getPtr());
        }

        m_gltfNodes = model.nodes;
        m_gltfRootNodes = model.rootNodes;

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

SafePtr<GameObject> Model::CreateGameObject(Model* model, Scene* scene, GameObject* parent)
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    SafePtr<GameObject> root = scene->CreateGameObject(parent);
    root->SetName(model->GetName());

    // GLTF: rebuild the full node hierarchy with local transforms
    if (!model->m_gltfNodes.empty())
    {
        std::vector<SafePtr<GameObject>> nodeObjects(model->m_gltfNodes.size());

        std::function<void(int, GameObject*)> buildNode =
            [&](int nodeIdx, GameObject* parentGO)
        {
            const GLTFLoader::Node& node = model->m_gltfNodes[nodeIdx];

            SafePtr<GameObject> go = scene->CreateGameObject(parentGO);
            go->SetName(node.name.empty() ? model->GetName() : node.name);
            nodeObjects[nodeIdx] = go;

            auto transform = go->GetTransform();
            transform->SetLocalPosition(node.transform.translation);
            transform->SetLocalRotation(node.transform.rotation);
            transform->SetLocalScale(node.transform.scale);

            if (node.meshIndex >= 0 && node.meshIndex < static_cast<int>(model->m_meshes.size()))
            {
                SafePtr<MeshComponent> meshComp = go->AddComponent<MeshComponent>();
                meshComp->SetMesh(model->m_meshes[node.meshIndex]);

                size_t materialOffset = 0;
                for (int mi = 0; mi < node.meshIndex; ++mi)
                    if (mi < static_cast<int>(model->m_meshes.size()))
                        materialOffset += model->m_meshes[mi]->GetSubMeshes().size();

                const auto& subMeshes = model->m_meshes[node.meshIndex]->GetSubMeshes();
                for (size_t j = 0; j < subMeshes.size(); ++j)
                {
                    const size_t matIdx = materialOffset + j;
                    if (matIdx < model->m_materials.size() && model->m_materials[matIdx].valid())
                        meshComp->AddMaterial(model->m_materials[matIdx].get());
                    else
                        meshComp->AddMaterial(resourceManager->GetDefaultMaterial());
                }
            }

            for (int childIdx : node.children)
                buildNode(childIdx, go.getPtr());
        };

        if (!model->m_gltfRootNodes.empty())
        {
            for (int rootIdx : model->m_gltfRootNodes)
                buildNode(rootIdx, root.getPtr());
        }
        else
        {
            for (size_t ni = 0; ni < model->m_gltfNodes.size(); ++ni)
                if (model->m_gltfNodes[ni].parent == -1)
                    buildNode(static_cast<int>(ni), root.getPtr());
        }

        return root;
    }

    size_t materialIndex = 0;
    for (size_t i = 0; i < model->m_meshes.size(); i++)
    {
        SafePtr<GameObject> child;
        if (model->m_meshes.size() >= 2)
            child = scene->CreateGameObject(root.getPtr());
        else
            child = root;

        child->SetName(model->m_meshes[i]->GetName());
        SafePtr<MeshComponent> meshComp = child->AddComponent<MeshComponent>();
        const auto subMeshes = model->m_meshes[i]->GetSubMeshes();
        for (size_t j = 0; j < subMeshes.size(); j++)
        {
            if (materialIndex >= model->m_materials.size())
            {
                meshComp->AddMaterial(resourceManager->GetDefaultMaterial());
                continue;
            }
            if (!model->m_materials[materialIndex].valid())
            {
                meshComp->AddMaterial(resourceManager->GetDefaultMaterial());
                continue;
            }
            meshComp->AddMaterial(model->m_materials[materialIndex++].get());
        }
        meshComp->SetMesh(model->m_meshes[i]);
    }
    return root;
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
