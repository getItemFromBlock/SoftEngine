#include "IResource.h"

#include "ResourceManager.h"
IResource::IResource(const std::filesystem::path& path)
{
    p_path = ResourceManager::SanitizePath(path);
}
