#include "IResource.h"

#include "ResourceManager.h"
IResource::IResource(const std::filesystem::path& path)
{
    p_path = ResourceManager::SanitizePath(path);
}

IResource::~IResource()
{
    // PrintLog("Resource %s destroyed", p_path.generic_string().c_str());
}

void IResource::Unload()
{
    p_state = ResourceState::Unload;
    EOnLoaded.Reset();
    EOnSentToGPU.Reset();
}

std::string IResource::GetName(bool extension) const
{
    if (!extension)
        return p_path.filename().stem().generic_string();
    return p_path.filename().generic_string();
}
