#pragma once
#include <filesystem>
#include <vector>
#include <optional>

#include <galaxymath/Maths.h>

#include "MTLLoader.h"

class OBJLoader
{
public:
    struct SubMesh
    {
        uint32_t startIndex;
        uint32_t count;
        std::optional<std::string> materialName;
    };

    struct Mesh
    {
        std::filesystem::path name;
        std::vector<SubMesh> subMeshes;
        std::vector<Vec3f> positions;
        std::vector<Vec2f> textureUVs;
        std::vector<Vec3f> normals;
        std::vector<Vec3f> tangents;
        std::vector<Vec3i> indices;
        std::vector<float> finalVertices;
    };

    struct Model
    {
        std::filesystem::path path;
        std::vector<Mesh> meshes;
        std::vector<MTLLoader::Material> materials;
    };

    static bool Load(const std::filesystem::path& fullPath, Model& model);

private:
    static bool Parse(Model& model);

    static void ParseFaceIndex(Vec3i& indices, const std::string& indexStr);

    static void ComputeVertices(Mesh& mesh);
};
