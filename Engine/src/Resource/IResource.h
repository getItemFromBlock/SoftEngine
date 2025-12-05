#pragma once
#include <atomic>
#include <filesystem>
#include <string>

#include "Core/UUID.h"
#include "Utils/Event.h"

class ResourceManager;
class RHIRenderer;

class IResource
{
public:
    IResource(const std::filesystem::path& path) : p_path(path.generic_string()) {}
    IResource(const IResource&) = delete;
    IResource(IResource&&) = delete;
    IResource& operator=(const IResource&) = delete;
    virtual ~IResource() = default;

    virtual bool Load(ResourceManager* resourceManager) = 0;
    virtual bool SendToGPU(RHIRenderer* renderer) = 0;
    virtual void Unload() = 0;

    UUID GetUUID() const { return p_uuid; }
    std::filesystem::path GetPath() const { return p_path; }
    bool IsLoaded() const { return p_isLoaded; }
    bool SentToGPU() const { return p_sendToGPU; }
    
    void SetLoaded()
    {
        OnLoaded.Invoke();
        p_isLoaded = true;
    }
    void SetSentToGPU()
    {
        OnSentToGPU.Invoke();
        p_sendToGPU = true;
    }
    
public:
    Event<> OnLoaded;
    Event<> OnSentToGPU;
protected:    
    std::filesystem::path p_path;
    UUID p_uuid;

    std::atomic_bool p_isLoaded;
    std::atomic_bool p_sendToGPU;
};
