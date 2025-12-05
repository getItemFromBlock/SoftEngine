#pragma once
#include <memory>
#include <unordered_map>

#include "Core/ThreadPool.h"

#include "Debug/Log.h"

#include "Render/RHI/RHIRenderer.h"

#include "Utils/Type.h"

#include "Resource/IResource.h"

#define RESOURCE_PATH "Engine/resources/"


class Shader;
class Texture;
class RHIRenderer;

class ResourceManager
{
public:
    ResourceManager() = default;
    
    void Initialize(RHIRenderer* renderer);
    
    template<typename T>
    std::shared_ptr<T> GetResource(const std::filesystem::path& path) const
    {
        auto it = m_resources.find(GetHash(path));
        if (it !=  m_resources.end())
        {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        return nullptr;
    }
    
    template <typename T>
    std::shared_ptr<T> GetResource(uint64_t hash) const
    {
        auto it = m_resources.find(hash);
        if (it !=  m_resources.end())
        {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        return nullptr;
    }

    template<typename T>
    SafePtr<T> AddResource(std::shared_ptr<T> resource)
    {
         m_resources[GetHash(resource->GetPath())] = resource;
        return resource;
    }

    template<typename T>
    void RemoveResource(uint64_t hash)
    {
        auto it =  m_resources.find(hash);
        if (it !=  m_resources.end())
        {
             m_resources.erase(it);
        }
    }

    template<typename T>
    SafePtr<T> Load(const std::filesystem::path& resourcePath, bool multiThread = true)
    {
        static_assert(std::is_base_of_v<IResource, T>, "T must inherit...");

        uint64_t hash = GetHash(resourcePath);

        auto it =  m_resources.find(hash);
        if (it !=  m_resources.end())
        {
            return std::dynamic_pointer_cast<T>(it->second);
        }

        std::shared_ptr<T> resource = std::make_shared<T>(resourcePath);
        AddResource(resource);
        if (multiThread)
        {
            ThreadPool::Enqueue([this, hash]() {
                auto resource = GetResource<T>(hash);
                if (!resource || resource->IsLoaded())
                    return;
                if (resource->Load(this))
                {
                    PrintLog("Resource %s loaded", resource->GetPath().generic_string().c_str());
                    resource->SetLoaded();
                    AddResourceToSend(hash);
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
                    AddResourceToSend(hash);
                }
            }
        }
        return resource;
    }
    
    void UpdateResourceToSend()
    {
        if (m_resourceToSend.empty())
            return;
        
        uint64_t hash = m_resourceToSend.front();
        m_resourceToSend.pop();
        
        auto it = m_resources.find(hash);
        if (it !=  m_resources.end())
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
                    AddResourceToSend(hash);
                }
            }
        }
    }
    
    void AddResourceToSend(uint64_t hash)
    {
        if (m_renderer->MultiThreadSendToGPU())
        {
            ThreadPool::Enqueue([hash, this]() {
                std::shared_ptr<IResource> resource = GetResource<IResource>(hash);
                if (resource && resource->SendToGPU(m_renderer))
                {
                    resource->SetSentToGPU();
                }
                else
                {
                    AddResourceToSend(hash);
                }
            });
        }
        else
        {
            m_resourceToSend.push(hash);
        }
    }
    
    void AddResourceToSend(const IResource* resource)
    {
        uint64_t hash = GetHash(resource->GetPath());
        AddResourceToSend(hash);
    }
    
    void Clear()
    {
        m_resources.clear();
    }
    
    void LoadDefaultShader(const std::filesystem::path& shaderPath);
    void LoadDefaultTexture(const std::filesystem::path& texturePath);
    
    std::shared_ptr<Shader> GetDefaultShader() const;
    std::shared_ptr<Texture> GetDefaultTexture() const;

private:
    static uint64_t GetHash(const std::filesystem::path& resourcePath)
    {
        return std::filesystem::hash_value(resourcePath);
    }
    
    RHIRenderer* m_renderer;
    std::unordered_map<uint64_t, std::shared_ptr<IResource>> m_resources;
    std::queue<uint64_t> m_resourceToSend;
    
    uint64_t m_defaultTexture;
    uint64_t m_defaultShader;
};
