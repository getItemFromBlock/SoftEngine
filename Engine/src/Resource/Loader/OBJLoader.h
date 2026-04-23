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

    struct Vertex
    {
        Vec3f position;
        Vec2f texCoord;
        Vec3f normal;
        Vec4f tangent;
    };

    struct VertexHash
    {
        size_t operator()(const Vertex& k) const
        {
            size_t pos = ((std::hash<float>()(k.position.x)
                ^ (std::hash<float>()(k.position.y) << 1)) >> 1)
                ^ (std::hash<float>()(k.position.z) << 1);
            size_t nor = ((std::hash<float>()(k.normal.x)
                ^ (std::hash<float>()(k.normal.y) << 1)) >> 1)
                ^ (std::hash<float>()(k.normal.z) << 1);
            size_t uv = std::hash<float>()(k.texCoord.x) ^
                (std::hash<float>()(k.texCoord.y) << 1);

            return ((std::hash<float>()((float)pos)
                ^ (std::hash<float>()((float)nor) << 1)) >> 1)
                ^ (std::hash<float>()((float)uv) << 1);
        }
    };

    struct VertexEqual
    {
        bool operator()(const Vertex &lhs, const Vertex &rhs) const
        {
            return lhs.position == rhs.position && lhs.texCoord == rhs.texCoord && lhs.normal == rhs.normal;
        }
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
        std::vector<float> intermediateVertices;
        std::vector<uint32_t> finalIndices;
        std::vector<Vertex> finalVertices;
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
