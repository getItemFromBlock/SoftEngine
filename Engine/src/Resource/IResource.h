#pragma once
#include <atomic>
#include <filesystem>
#include <string>

#include "Core/UUID.h"
#include "Scene/ClassDescriptor.h"
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

#define DECLARE_RESOURCE_TYPE_PARENT(T, U) \
    T(std::filesystem::path path) : U(std::move(path)) {} \
    T(const T&) = delete; \
    T(T&&) = delete; \
    T& operator=(const T&) = delete; \
    ~T() override = default; \
    static ResourceType GetStaticResourceType() { return ResourceType::T; } \
    virtual ResourceType GetResourceType() const override { return GetStaticResourceType(); }

#define DECLARE_RESOURCE_TYPE(T) DECLARE_RESOURCE_TYPE_PARENT(T, IResource)

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
    
    virtual void Describe(ClassDescriptor& descriptor) {}
    
    virtual ResourceType GetResourceType() const = 0;

    Core::UUID GetUUID() const { return p_uuid; }
    std::filesystem::path GetPath() const { return p_path; }

    virtual std::string GetName(bool extension = false) const;
    bool IsLoaded() const { return p_isLoaded; }
    bool SentToGPU() const { return p_sendToGPU; }
    
    void SetLoaded()
    {
        p_isLoaded = true;
        EOnLoaded.Invoke();
    }
    void SetSentToGPU()
    {
        p_sendToGPU = true;
        EOnSentToGPU.Invoke();
    }
    
public:
    OnceEvent EOnLoaded;
    OnceEvent EOnSentToGPU;
protected:    
    friend class ResourceManager;
    
    std::filesystem::path p_path;
    Core::UUID p_uuid;

    std::atomic_bool p_isLoaded;
    std::atomic_bool p_sendToGPU;
};
