#include "MTLLoader.h"

#include <filesystem>
#include <fstream>
#include <iosfwd>

std::vector<MTLLoader::Material> MTLLoader::Load(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return {};
    }

    std::string line;
    Material currentMaterial;
    bool first = true;
    std::vector<MTLLoader::Material> materials;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string token;
        iss >> token;
        // Ambient
        if (token == "newmtl")
        {
            if (!first)
            {
                materials.push_back(currentMaterial);
                currentMaterial = Material();
            }
            first = false;
            std::string name;
            iss >> currentMaterial.name;
        }
        if (token == "Ka")
        {
            Vec4f ambient(1.f);
            iss >> ambient.x >> ambient.y >> ambient.z;
            currentMaterial.ambient = ambient;
        }
        // Diffuse
        else if (token == "Kd")
        {
            Vec4f diffuse(1.f);
            iss >> diffuse.x >> diffuse.y >> diffuse.z;
            currentMaterial.diffuse = diffuse;
        }
        // Specular
        else if (token == "Ks")
        {
            Vec4f specular(1.f);
            iss >> specular.x >> specular.y >> specular.z;
            currentMaterial.specular = specular;
        }
        // Emissive
        else if (token == "d")
        {
            float transparency;
            iss >> transparency;
            currentMaterial.transparency = transparency;
        }
        else if (token == "map_Kd")
        {
            std::filesystem::path albedo;
            iss >> albedo;
            currentMaterial.albedo = albedo;
        }
    }
    if (!currentMaterial.name.empty())
        materials.push_back(currentMaterial);
    return materials;
}