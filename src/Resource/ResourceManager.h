#pragma once
#include <memory>
#include <unordered_map>

#include "Core/UUID.h"
#include "Core/ThreadPool.h"
#include "Debug/Log.h"
#include "Render/RHI/RHIRenderer.h"

#include "Utils/Type.h"

#include "Resource/IResource.h"
#include "Resource/Model.h"


class RHIRenderer;

class ResourceManager
{
public:
    ResourceManager() = default;
    
    void Initialize(RHIRenderer* renderer);
    
    template<typename T>
    std::shared_ptr<T> GetResource(const std::filesystem::path& path)
    {
        auto it = m_resources.find(GetHash(path));
        if (it !=  m_resources.end())
        {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        return nullptr;
    }
    
    template <typename T>
    std::shared_ptr<T> GetResource(uint64_t hash)
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
    SafePtr<T> Load(const std::filesystem::path& resourcePath)
    {
        static_assert(std::is_base_of_v<IResource, T>, "T must inherit...");

        uint64_t hash = GetHash(resourcePath);

        auto it =  m_resources.find(hash);
        if (it !=  m_resources.end())
        {
            return std::dynamic_pointer_cast<T>(it->second);
        }

        std::shared_ptr<T> resource = std::make_shared<T>(resourcePath);
        ThreadPool::Enqueue([resource, this, hash]() {
            if (resource->IsLoaded())
                return;
            if (resource->Load(this))
            {
                resource->SetLoaded();
                AddResourceToSend(hash);
            }
        });
        AddResource(resource);
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
            }
        }
    }
    
    void AddResourceToSend(uint64_t hash)
    {
        if (m_renderer->MultiThreadSendToGPU())
        {
            std::shared_ptr<IResource> resource = GetResource<IResource>(hash);
            ThreadPool::Enqueue([resource, this]() {
                if (resource->SendToGPU(m_renderer))
                {
                    resource->SetSentToGPU();
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
private:
    static uint64_t GetHash(const std::filesystem::path& resourcePath)
    {
        return std::filesystem::hash_value(resourcePath);
    }
    
    RHIRenderer* m_renderer;
    std::unordered_map<uint64_t, std::shared_ptr<IResource>> m_resources;
    std::queue<uint64_t> m_resourceToSend;
};
