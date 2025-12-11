#pragma once
#include <memory>
#include <unordered_map>

#include "Core/ThreadPool.h"

#include "Debug/Log.h"

#include "Render/RHI/RHIRenderer.h"

#include "Utils/Type.h"

#include "Resource/IResource.h"

#define RESOURCE_PATH "Engine/resources/"


class Material;
class Shader;
class Texture;
class RHIRenderer;

class ResourceManager
{
public:
    ResourceManager() = default;

    void Initialize(RHIRenderer* renderer);

    Core::UUID GetUUID(const std::filesystem::path& resourcePath) const;

    template<typename T>
    std::shared_ptr<T> GetResource(const std::filesystem::path& resourcePath) const;

    template<typename T>
    std::shared_ptr<T> GetResource(const Core::UUID& uuid) const;
    
    bool Contains(const Core::UUID& uuid) const;

    template<typename T>
    SafePtr<T> AddResource(const std::shared_ptr<T>& resource);

    void AddResource(const Core::UUID& uuid, const std::shared_ptr<IResource>& resource, uint64_t hash);

    void RemoveResource(Core::UUID uuid);

    static std::filesystem::path SanitizePath(const std::filesystem::path& resourcePath);

    template<typename T>
    SafePtr<T> Load(const std::filesystem::path& resourcePath, bool multiThread = true);

    static std::shared_ptr<IResource> CreateResourceFromPath(const std::filesystem::path& path);

    void UpdateResourceToSend();

    void AddResourceToSend(Core::UUID uuid);

    void AddResourceToSend(const IResource* resource);

    void Clear();

    void LoadDefaultShader(const std::filesystem::path& shaderPath);
    void LoadDefaultTexture(const std::filesystem::path& texturePath);
    void LoadDefaultMaterial(const std::filesystem::path& materialPath);

    SafePtr<Material> CreateMaterial(const std::filesystem::path& path);

    std::shared_ptr<Shader> GetDefaultShader() const;
    std::shared_ptr<Texture> GetDefaultTexture() const;
    std::shared_ptr<Material> GetDefaultMaterial() const;

    static std::filesystem::path GetCacheDir();
    static std::filesystem::path GetCompiledCacheDir();

    void ReadCache();
    void CreateCache();

private:
    using Hash = uint64_t;
    static Hash GetHash(const std::filesystem::path& resourcePath)
    {
        return std::filesystem::hash_value(resourcePath);
    }

    static void CreateCacheDir();

private:
    RHIRenderer* m_renderer;
    std::unordered_map<Core::UUID, std::shared_ptr<IResource>> m_resources;
    std::unordered_map<Hash, Core::UUID> m_hashToUUID;
    std::queue<Core::UUID> m_resourceToSend;

    Core::UUID m_defaultTexture;
    Core::UUID m_defaultShader;
    Core::UUID m_defaultMaterial;
};
template<typename T>
std::shared_ptr<T> ResourceManager::GetResource(const std::filesystem::path& resourcePath) const
{
    std::filesystem::path path = SanitizePath(resourcePath);
    Core::UUID uuid = GetUUID(path);
    if (uuid == UUID_INVALID)
        return nullptr;

    return GetResource<T>(uuid);
}
template<typename T>
std::shared_ptr<T> ResourceManager::GetResource(const Core::UUID& uuid) const
{
    auto it = m_resources.find(uuid);
    if (it == m_resources.end())
    {
        return nullptr;
    }
    return std::dynamic_pointer_cast<T>(it->second);;
}
template<typename T>
SafePtr<T> ResourceManager::AddResource(const std::shared_ptr<T>& resource)
{
    Hash hash = GetHash(resource->GetPath());
    auto it = m_hashToUUID.find(hash);
    
    if (it != m_hashToUUID.end())
    {
        // Remove old resource
        auto prevUUID = it->second;
        RemoveResource(prevUUID);
    }
    
    AddResource(resource->GetUUID(), resource, hash);
    return resource;
}
template<typename T>
SafePtr<T> ResourceManager::Load(const std::filesystem::path& resourcePath, bool multiThread)
{
    static_assert(std::is_base_of_v<IResource, T>, "T must inherit...");

    std::filesystem::path path = SanitizePath(resourcePath);

    Core::UUID uuid = GetUUID(path);

    std::shared_ptr<T> resource = nullptr;
    if (uuid == UUID_INVALID)
    {
        resource = std::make_shared<T>(path);
        AddResource(resource);
        uuid = resource->GetUUID();
    }
    else
    {
        resource = GetResource<T>(uuid);
        if (!resource)
        {
            resource = std::make_shared<T>(path);
            resource->p_uuid = uuid;
            AddResource(resource);
        }
    }

    if (multiThread)
    {
        ThreadPool::Enqueue([this, uuid]()
        {
            auto resource = GetResource<T>(uuid);
            if (!resource || resource->IsLoaded())
            {
                PrintError("Resource is invalid");
                return;
            }
            if (resource->Load(this))
            {
                PrintLog("Resource %s loaded", resource->GetPath().generic_string().c_str());
                resource->SetLoaded();
                AddResourceToSend(uuid);
            }
        });
    }
    else
    {
        if (resource->Load(this))
        {
            PrintLog("Resource %s loaded", resource->GetPath().generic_string().c_str());
            resource->SetLoaded();
            if (resource->SendToGPU(m_renderer))
            {
                resource->SetSentToGPU();
            }
            else
            {
                AddResourceToSend(uuid);
            }
        }
    }
    return resource;
}
