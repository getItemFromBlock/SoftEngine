#include "ResourceManager.h"

#include <ranges>
#include <iomanip>

#include "FragmentShader.h"
#include "Material.h"
#include "Texture.h"
#include "Shader.h"
#include "Model.h"
#include "Mesh.h"
#include "VertexShader.h"
#include "Utils/File.h"

class Shader;

void ResourceManager::Initialize(RHIRenderer* renderer)
{
    m_renderer = renderer;
    CreateCacheDir();
    ReadCache();
}

Core::UUID ResourceManager::GetUUID(const std::filesystem::path& resourcePath) const
{
    std::filesystem::path path = SanitizePath(resourcePath);
    auto it = m_hashToUUID.find(GetHash(path));
    if (it != m_hashToUUID.end())
    {
        return it->second;
    }
    return UUID_INVALID;
}

bool ResourceManager::Contains(const Core::UUID& uuid) const
{
    return m_resources.contains(uuid);
}

void ResourceManager::AddResource(const Core::UUID& uuid, const std::shared_ptr<IResource>& resource, Hash hash)
{
    m_resources[uuid] = resource;
    m_hashToUUID[hash] = uuid;
}

void ResourceManager::RemoveResource(Core::UUID uuid)
{
    auto it = m_resources.find(uuid);
    if (it != m_resources.end())
    {
        m_resources.erase(it);
    }
}

std::filesystem::path ResourceManager::SanitizePath(const std::filesystem::path& resourcePath)
{
    return resourcePath.lexically_normal();
}

std::shared_ptr<IResource> ResourceManager::CreateResourceFromPath(const std::filesystem::path& path)
{
    std::string extension = path.extension().generic_string();
    extension = extension.substr(1); // Remove the . for optimization

    std::ranges::transform(extension, extension.begin(), ::tolower);
    auto it = extensionToResourceType.find(extension);

    if (it == extensionToResourceType.end())
        return nullptr;

    switch (it->second)
    {
        case ResourceType::Texture:
            return std::make_shared<Texture>(path);
        case ResourceType::Mesh:
            return std::make_shared<Mesh>(path);
        case ResourceType::Model:
            return std::make_shared<Model>(path);
        case ResourceType::FragmentShader:
            return std::make_shared<FragmentShader>(path);
        case ResourceType::VertexShader:
            return std::make_shared<VertexShader>(path);
        case ResourceType::Shader:
            return std::make_shared<Shader>(path);
        case ResourceType::Material:
            return std::make_shared<Material>(path);
        default:
            return nullptr;
    }
}
void ResourceManager::UpdateResourceToSend()
{
    if (m_resourceToSend.empty())
        return;

    Core::UUID uuid = m_resourceToSend.front();
    m_resourceToSend.pop();

    auto it = m_resources.find(uuid);
    if (it != m_resources.end())
    {
        if (it->second->IsLoaded() && !it->second->SentToGPU())
        {
            if (it->second->SendToGPU(m_renderer))
            {
                PrintLog("Resource sent to GPU: %s", it->second->GetPath().generic_string().c_str());
                it->second->SetSentToGPU();
            }
            else
            {
                AddResourceToSend(uuid);
            }
        }
    }
}
void ResourceManager::AddResourceToSend(Core::UUID uuid)
{
    if (m_renderer->MultiThreadSendToGPU())
    {
        ThreadPool::Enqueue([uuid, this]()
        {
            std::shared_ptr<IResource> resource = GetResource<IResource>(uuid);
            if (resource && resource->SendToGPU(m_renderer))
            {
                resource->SetSentToGPU();
            }
            else
            {
                AddResourceToSend(uuid);
            }
        });
    }
    else
    {
        m_resourceToSend.push(uuid);
    }
}
void ResourceManager::AddResourceToSend(const IResource* resource)
{
    AddResourceToSend(resource->GetUUID());
}
void ResourceManager::Clear()
{
    m_resources.clear();
}

void ResourceManager::LoadDefaultShader(const std::filesystem::path& shaderPath)
{
    SafePtr<Shader> shader = Load<Shader>(shaderPath, false);

    m_defaultShader = shader->GetUUID();
}

void ResourceManager::LoadDefaultTexture(const std::filesystem::path& texturePath)
{
    SafePtr<Texture> texture = Load<Texture>(texturePath, false);

    m_defaultTexture = texture->GetUUID();

    m_renderer->SetDefaultTexture(texture);
}

void ResourceManager::LoadDefaultMaterial(const std::filesystem::path& materialPath)
{
    SafePtr<Material> material = CreateMaterial(materialPath);

    m_defaultMaterial = material->GetUUID();
}

SafePtr<Material> ResourceManager::CreateMaterial(const std::filesystem::path& path)
{
    std::shared_ptr<Material> material = std::make_shared<Material>(path);
    std::shared_ptr<Shader> shader = GetDefaultShader();
    material->SetShader(shader);

    AddResource(material);

    return material;
}

std::shared_ptr<Shader> ResourceManager::GetDefaultShader() const
{
    return GetResource<Shader>(m_defaultShader);
}

std::shared_ptr<Texture> ResourceManager::GetDefaultTexture() const
{
    return GetResource<Texture>(m_defaultTexture);
}

std::shared_ptr<Material> ResourceManager::GetDefaultMaterial() const
{
    return GetResource<Material>(m_defaultMaterial);
}

std::filesystem::path ResourceManager::GetCacheDir()
{
    return "Engine/cache/";
}

std::filesystem::path ResourceManager::GetCompiledCacheDir()
{
    return GetCacheDir() / "compiled";
}

void ResourceManager::ReadCache()
{
    std::filesystem::path cachePath = GetCacheDir() / "resources.cache";

    std::ifstream stream(cachePath);

    if (!stream.is_open())
    {
        return; 
    }

    uint64_t uuid = 0;
    std::string pathString;

    while (stream >> uuid >> std::quoted(pathString))
    {
        AddResource(uuid, nullptr, GetHash(pathString));
    }
}

void ResourceManager::CreateCache()
{
    std::filesystem::path cachePath = GetCacheDir() / "resources.cache";
    std::ofstream stream(cachePath);

    if (!stream.is_open())
    {
        PrintError("Failed to create cache at %s", cachePath.generic_string().c_str());
        return;
    }

    for (const auto& resource : m_resources | std::views::values)
    {
        stream << resource->GetUUID() << " " << std::quoted(resource->GetPath().generic_string()) << "\n";
    }
}

void ResourceManager::CreateCacheDir()
{
    std::filesystem::create_directories(GetCacheDir());
    std::filesystem::create_directories(GetCompiledCacheDir());
}
