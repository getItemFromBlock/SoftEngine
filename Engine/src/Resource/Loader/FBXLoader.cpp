/*
#include "FBXLoader.h"
#include "ImageLoader.h"

#include "Resource/ResourceManager.h"
#include "Resource/Model.h"
#include "Resource/Mesh.h"
#include "Resource/Texture.h"
#include "Resource/Shader.h"

#include <openFBX/ofbx.h>
#include <stb/stb_image.h>

#include "Core/Engine.h"

const char* ReadFile(const char* filename, uint32_t& size, bool& success)
{
    std::ifstream is(filename, std::ifstream::binary);
    if (is.is_open())
    {
        success = true;
        // get length of file:
        is.seekg(0, is.end);
        uint32_t length = (uint32_t)is.tellg();
        is.seekg(0, is.beg);

        char* buffer = new char[length];

        // read data as a block:
        is.read(buffer, length);
        is.close();
        size = length;
        return buffer;
    }
    else
    {
        success = false;
        PrintWarning("File %s cannot be found", filename);
        return 0;
    }
}

Vec4f ToVec4f(const ofbx::Color& c)
{
    return {c.r, c.g, c.b, 1};
}

Vec3f ToVec3f(const ofbx::Vec3& v)
{
    return {v.x, v.y, v.z};
}

Vec2f ToVec2f(const ofbx::Vec2& v)
{
    return {v.x, v.y};
}

Quat ToQuat(const ofbx::Quat& q)
{
    return {q.x, q.y, q.z, q.w};
}

void FBXLoader::Load(const std::filesystem::path& fullPath, Model* outputModel, ResourceManager* resourceManager)
{
    uint32_t size;
    bool sucess;
    auto data = (ofbx::u8*)ReadFile(fullPath.generic_string().c_str(), size, sucess);
    if (!sucess)
    {
        delete[] data;
        data = nullptr;
        return;
    }
    ofbx::IScene* Scene = ofbx::load(data, size, (ofbx::u16)ofbx::LoadFlags::NONE);
    if (Scene)
    {
        LoadTextures(Scene, fullPath, resourceManager);
        LoadModel(Scene, fullPath, outputModel, resourceManager);
        Scene->destroy();
    }
    delete[] data;
    data = nullptr;
}

void FBXLoader::LoadTextures(ofbx::IScene* fbxScene, const std::filesystem::path& fullPath, ResourceManager* resourceManager)
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    for (int i = 0; i < fbxScene->getEmbeddedDataCount(); i++)
    {
        constexpr int size = 4096;
        char tmp[size];
        fbxScene->getEmbeddedFilename(i).toString(tmp);
        std::filesystem::path texPath = tmp;
        if (!std::filesystem::exists(texPath))
        {
            const ofbx::DataView& embeddedData = fbxScene->getEmbeddedData(i); // Assuming this function exists.

            const ofbx::u8* textureData = embeddedData.begin + 4;
            std::size_t textureSize = static_cast<std::size_t>(embeddedData.end - embeddedData.begin - 4);

            ImageLoader::Image image = ImageLoader::LoadFromMemory(const_cast<unsigned char*>(textureData),
                                                                   static_cast<int>(textureSize));

            std::shared_ptr<Texture> texture = std::make_shared<Texture>(texPath);
            resourceManager->AddResource<Texture>(texture);
            texture->CreateFromData(image);

            const bool exportImage = true;
            if (exportImage)
            {
                std::filesystem::create_directories(texPath.parent_path());
                ImageLoader::SaveImage(texPath.string().c_str(), image);
            }
        }
        else
        {
            resourceManager->Load<Texture>(texPath);
        }
    }
}

std::vector<float> ComputeVertices(std::vector<Vec3f> positions, std::vector<Vec2f> textureUVs,
                                   std::vector<Vec3f> normals, std::vector<Vec3i> indices)
{
    std::vector<Vec3f> tangents;
    tangents.resize(positions.size(), { 0.0f, 0.0f, 0.0f });
    std::vector<float> finalVertices;

    for (size_t k = 0; k < indices.size(); k += 3)
    {
        const Vec3i& idx0 = indices[k];
        const Vec3i& idx1 = indices[k + 1];
        const Vec3i& idx2 = indices[k + 2];

        const Vec3f& Edge1 = positions[idx1.x] - positions[idx0.x];
        const Vec3f& Edge2 = positions[idx2.x] - positions[idx0.x];

        const float DeltaU1 = textureUVs[idx1.y].x - textureUVs[idx0.y].x;
        const float DeltaV1 = textureUVs[idx1.y].y - textureUVs[idx0.y].y;
        const float DeltaU2 = textureUVs[idx2.y].x - textureUVs[idx0.y].x;
        const float DeltaV2 = textureUVs[idx2.y].y - textureUVs[idx0.y].y;

        float f = DeltaU1 * DeltaV2 - DeltaU2 * DeltaV1;
        f = (fabs(f) < 1e-6f) ? 1.0f : 1.0f / f;

        Vec3f Tangent;
        Tangent.x = f * (DeltaV2 * Edge1.x - DeltaV1 * Edge2.x);
        Tangent.y = f * (DeltaV2 * Edge1.y - DeltaV1 * Edge2.y);
        Tangent.z = f * (DeltaV2 * Edge1.z - DeltaV1 * Edge2.z);

        tangents[idx0.x] = tangents[idx0.x] + Tangent;
        tangents[idx1.x] = tangents[idx1.x] + Tangent;
        tangents[idx2.x] = tangents[idx2.x] + Tangent;
    }

    for (auto& t : tangents)
        t.Normalize();

    for (size_t i = 0; i < indices.size(); i++)
    {
        const Vec3i& idx = indices[i];

        finalVertices.push_back(positions[idx.x].x);
        finalVertices.push_back(positions[idx.x].y);
        finalVertices.push_back(positions[idx.x].z);

        finalVertices.push_back(textureUVs[idx.y].x);
        finalVertices.push_back(textureUVs[idx.y].y);

        finalVertices.push_back(normals[idx.z].x);
        finalVertices.push_back(normals[idx.z].y);
        finalVertices.push_back(normals[idx.z].z);

        finalVertices.push_back(tangents[idx.x].x);
        finalVertices.push_back(tangents[idx.x].y);
        finalVertices.push_back(tangents[idx.x].z);
    }
}

void SetMaterialTexture(SafePtr<Texture>* texture, const ofbx::Material* fbxMaterial,
                        const std::filesystem::path& fullPath, ofbx::Texture::TextureType texType, ResourceManager* resourceManager)
{
    if (auto fbxTexture = fbxMaterial->getTexture(texType))
    {
        constexpr int size = 4096;
        char texName[size];
        fbxTexture->getRelativeFileName().toString(texName);
        std::filesystem::path texPath = fullPath.parent_path() / texName;
        
        *texture = resourceManager->Load<Texture>(texPath);
    }
}

void FBXLoader::LoadModel(ofbx::IScene* fbxScene, const std::filesystem::path& fullPath, Model* outputModel, ResourceManager* resourceManager)
{
    std::vector<std::vector<Vec3f>> allPositions;
    for (int i = 0; i < fbxScene->getMeshCount(); i++)
    {
        const ofbx::Mesh* fbxMesh = fbxScene->getMesh(i);

        // Load Materials
        size_t materialCount = fbxMesh->getMaterialCount();
        for (int j = 0; j < materialCount; j++)
        {
            const ofbx::Material* fbxMaterial = fbxMesh->getMaterial(j);
            const std::filesystem::path& materialFullPath = fullPath.parent_path() / (std::string(fbxMaterial->name) +
                ".mat");
            SafePtr material = resourceManager->GetResource<Material>(materialFullPath);

            if (!material)
            {
                material = resourceManager->CreateMaterial(materialFullPath).get();

                SafePtr<Texture> albedo;
                SafePtr<Texture> normal;
                SetMaterialTexture(&albedo, fbxMaterial, fullPath, ofbx::Texture::TextureType::DIFFUSE, resourceManager);
                SetMaterialTexture(&normal, fbxMaterial, fullPath, ofbx::Texture::TextureType::NORMAL, resourceManager);
                
                if (albedo)
                    material->SetAttribute("albedoSampler", albedo);
                if (normal)
                    material->SetAttribute("normalSampler", normal);
            }
            outputModel->m_materials.emplace_back(material);
        }

        const char* name = fbxMesh->name;
        const std::filesystem::path& meshFullPath = fullPath / (std::string(name) + ".mesh");
        SafePtr mesh = resourceManager->GetResource<Mesh>(meshFullPath);

        if (!mesh)
        {
            // If mesh not in resource manager
            std::shared_ptr<Mesh> copy = std::make_shared<Mesh>(meshFullPath);
            mesh = resourceManager->AddResource<Mesh>(copy).lock();
        }


        std::vector<float> finalVertices;
        std::vector<Vec3f> positions;

        auto fbxPositions = fbxMesh->getGeometryData().getPositions();
        auto fbxTextureUVs = fbxMesh->getGeometryData().getUVs();
        auto fbxNormals = fbxMesh->getGeometryData().getNormals();

        const auto pushToVector = [&](int index)
        {
            positions.push_back(ToVec3f(fbxPositions.get(index)));

            finalVertices.push_back(fbxPositions.get(index).x);
            finalVertices.push_back(fbxPositions.get(index).y);
            finalVertices.push_back(fbxPositions.get(index).z);

            finalVertices.push_back(fbxTextureUVs.get(index).x);
            finalVertices.push_back(fbxTextureUVs.get(index).y);

            finalVertices.push_back(fbxNormals.get(index).x);
            finalVertices.push_back(fbxNormals.get(index).y);
            finalVertices.push_back(fbxNormals.get(index).z);

            finalVertices.push_back(0);
            finalVertices.push_back(0);
            finalVertices.push_back(0);
        };

        int totalVertexCount = 0;
        size_t totalVertexCountSub = 0;
        auto partitionCount = fbxMesh->getGeometryData().getPartitionCount();
        for (int j = 0; j < partitionCount; j++)
        {
            SubMesh subMesh;
            subMesh.startIndex = totalVertexCount;

            auto currentPartition = fbxMesh->getGeometryData().getPartition(j);
            int tris = currentPartition.triangles_count;

            for (size_t k = 0; k < currentPartition.polygon_count; k++)
            {
                auto currentPolygon = currentPartition.polygons[k];
                if (currentPolygon.vertex_count == 3)
                {
                    for (int l = totalVertexCount; l < totalVertexCount + currentPolygon.vertex_count; l++)
                    {
                        pushToVector(l);
                        totalVertexCountSub += 1;
                    }
                }
                else if (currentPolygon.vertex_count == 4)
                {
                    pushToVector(totalVertexCount);
                    pushToVector(totalVertexCount + 1);
                    pushToVector(totalVertexCount + 2);

                    pushToVector(totalVertexCount + 2);
                    pushToVector(totalVertexCount + 3);
                    pushToVector(totalVertexCount);
                    totalVertexCountSub += 6;
                }
                totalVertexCount += currentPolygon.vertex_count;
            }
            subMesh.count = totalVertexCountSub - subMesh.startIndex;
            mesh->m_subMeshes.push_back(subMesh);
        }

        allPositions.push_back(positions);

        mesh->m_finalVertices = finalVertices;

        mesh->p_shouldBeLoaded = true;
        mesh->p_loaded = true;

        outputModel->m_meshes.push_back(mesh);

        mesh->m_model = outputModel;

        auto bind = [outputModel] { outputModel->OnMeshLoaded(); };
        mesh->OnLoad.Bind(bind);

        mesh->SendRequest();
    }
    outputModel->ComputeBoundingBox(allPositions);
    outputModel->p_hasBeenSent = true;
}
*/
