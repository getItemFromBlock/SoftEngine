#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <galaxymath/Maths.h>

class GLTFLoader
{
public:
    struct SubMesh
    {
        uint32_t startIndex;
        uint32_t count;
        std::optional<std::string> materialName;
    };

    struct Material
    {
        std::filesystem::path name;
        Vec4f color = Vec4f::One();
        float roughnessFactor = 1.f;
        float metallicFactor = 1.f;
        std::optional<std::filesystem::path> albedo;
        std::optional<std::filesystem::path> normal;
        std::optional<std::filesystem::path> metallic;
        std::optional<std::filesystem::path> roughness;
    };

    struct Mesh
    {
        std::filesystem::path name;
        std::vector<SubMesh> subMeshes;
        std::vector<Vec3f> positions;
        std::vector<Vec2f> textureUVs;
        std::vector<Vec3f> normals;
        std::vector<Vec4f> tangents;
        std::vector<Vec3i> indices;
        std::vector<float> finalVertices;
    };

    struct NodeTransform
    {
        Vec3f translation = {0.f, 0.f, 0.f};
        Quat rotation = Quat::Identity();
        Vec3f scale = {1.f, 1.f, 1.f};
    };

    struct Node
    {
        std::string name;
        int meshIndex = -1; // index into Model::meshes, -1 = no mesh
        NodeTransform transform;
        std::vector<int> children; // indices into Model::nodes
        int parent = -1; // -1 = root
    };

    struct Model
    {
        std::filesystem::path path;
        std::vector<Mesh> meshes;
        std::vector<Material> materials;
        std::vector<Node> nodes;
        std::vector<int> rootNodes;
    };

    static bool Load(const std::filesystem::path& fullPath, Model& model);
};
