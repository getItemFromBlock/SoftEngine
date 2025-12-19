#pragma once
#include <filesystem>
#include <vector>
#include <galaxymath/Maths.h>

class MTLLoader
{
public:
    MTLLoader() = default;

    struct Material
    {
        std::filesystem::path name;
        Vec3f ambient = Vec3f::Zero();
        Vec3f diffuse = Vec3f::One();
        Vec3f specular = Vec3f::Zero();
        Vec4f emissive;
        float transparency;
        std::optional<std::filesystem::path> albedo;
    };

    static std::vector<Material> Load(const std::filesystem::path& path);

private:
};
