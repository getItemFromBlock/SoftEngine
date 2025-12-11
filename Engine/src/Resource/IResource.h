#pragma once
#include <atomic>
#include <filesystem>
#include <string>

#include "Core/UUID.h"
#include "Utils/Event.h"

class ResourceManager;
class RHIRenderer;

enum class ResourceType
{
    None,
    Texture,
    Mesh,
    Model,
    FragmentShader,
    VertexShader,
    // ComputeShader,
    Shader,
    Material
};

inline const char* to_string(ResourceType e)
{
    switch (e)
    {
        case ResourceType::None: return "None";
        case ResourceType::Texture: return "Texture";
        case ResourceType::Mesh: return "Mesh";
        case ResourceType::Model: return "Model";
        case ResourceType::FragmentShader: return "FragmentShader";
        case ResourceType::VertexShader: return "VertexShader";
        case ResourceType::Shader: return "Shader";
        case ResourceType::Material: return "Material";
        default: return "unknown";
    }
}

inline static std::unordered_map<std::string, ResourceType> extensionToResourceType =
{
    { "png", ResourceType::Texture },
    { "jpeg", ResourceType::Texture },
    { "tga", ResourceType::Texture },
    { "bmp", ResourceType::Texture },
    { "psd", ResourceType::Texture },
    { "gif", ResourceType::Texture },
    { "obj", ResourceType::Model },
    { "vert", ResourceType::VertexShader },
    { "frag", ResourceType::FragmentShader },
    { "shader", ResourceType::Shader }
};

class IResource
{
public:
    IResource(const std::filesystem::path& path);
    IResource(const IResource&) = delete;
    IResource(IResource&&) = delete;
    IResource& operator=(const IResource&) = delete;
    virtual ~IResource() = default;

    virtual bool Load(ResourceManager* resourceManager) = 0;
    virtual bool SendToGPU(RHIRenderer* renderer) = 0;
    virtual void Unload() = 0;
    
    virtual ResourceType GetResourceType() const = 0;

    Core::UUID GetUUID() const { return p_uuid; }
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
    OnceEvent OnLoaded;
    OnceEvent OnSentToGPU;
protected:    
    friend class ResourceManager;
    
    std::filesystem::path p_path;
    Core::UUID p_uuid;

    std::atomic_bool p_isLoaded;
    std::atomic_bool p_sendToGPU;
};
