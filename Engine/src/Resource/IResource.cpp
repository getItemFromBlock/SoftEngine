#include "IResource.h"

#include "ResourceManager.h"
IResource::IResource(const std::filesystem::path& path)
{
    p_path = ResourceManager::SanitizePath(path);
}

std::string IResource::GetName(bool extension) const
{
    if (!extension)
        return p_path.filename().stem().generic_string();
    return p_path.filename().generic_string();
}
