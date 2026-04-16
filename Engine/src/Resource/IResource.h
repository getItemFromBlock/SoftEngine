#pragma once
#include <atomic>
#include <filesystem>
#include <string>

#include "Core/UUID.h"

#include "Scene/ClassDescriptor.h"

#include "Utils/Event.h"
#include "Utils/File.h"

class ResourceManager;
class VulkanRenderer;

enum class ResourceType
{
    None,
    Texture,
    Mesh,
    Model,
    FragmentShader,
    VertexShader,
    ComputeShader,
    Shader,
    Material,
    CubeMap,
    RenderTargetTexture,
    PostProcessShader,
    Count
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
    case ResourceType::ComputeShader: return "ComputeShader";
    case ResourceType::Shader: return "Shader";
    case ResourceType::Material: return "Material";
    case ResourceType::CubeMap: return "CubeMap";
    case ResourceType::RenderTargetTexture: return "RenderTargetTexture";
    case ResourceType::PostProcessShader: return "PostProcessShader";
    default: return "unknown";
    }
}

inline static std::unordered_map<std::string, ResourceType> extensionToResourceType =
{
    {"png", ResourceType::Texture},
    {"jpeg", ResourceType::Texture},
    {"tga", ResourceType::Texture},
    {"bmp", ResourceType::Texture},
    {"psd", ResourceType::Texture},
    {"gif", ResourceType::Texture},
    {"obj", ResourceType::Model},
    {"gltf", ResourceType::Model},
    {"mesh", ResourceType::Mesh},
    {"vert", ResourceType::VertexShader},
    {"frag", ResourceType::FragmentShader},
    {"comp", ResourceType::ComputeShader},
    {"shader", ResourceType::Shader},
    {"mat", ResourceType::Material},
    {"hdr", ResourceType::CubeMap},
    {"pshader", ResourceType::PostProcessShader, }
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

enum class ResourceState
{
    Unload = 0,
    Loading,
    Loaded,
    SendingToGPU,
    SentToGPU,
};

inline const char* to_string(ResourceState e)
{
    switch (e)
    {
    case ResourceState::Unload: return "Unload";
    case ResourceState::Loading: return "Loading";
    case ResourceState::Loaded: return "Loaded";
    case ResourceState::SendingToGPU: return "SendingToGPU";
    case ResourceState::SentToGPU: return "SentToGPU";
    default: return "unknown";
    }
}

class IResource : public IDescribe
{
public:
    IResource(const std::filesystem::path& path);
    IResource(const IResource&) = delete;
    IResource(IResource&&) = delete;
    IResource& operator=(const IResource&) = delete;
    virtual ~IResource();

    virtual bool Load(ResourceManager* resourceManager) = 0;
    virtual bool SendToGPU(VulkanRenderer* renderer) = 0;
    virtual void Unload();

    virtual void Describe(ClassDescriptor& descriptor) override
    {
    }

    virtual ResourceType GetResourceType() const = 0;

    Core::UUID GetUUID() const { return p_uuid; }
    std::filesystem::path GetPath() const { return p_path; }

    virtual bool Exists() const { return File::Exists(p_path); }
    virtual std::string GetName(bool extension = false) const;
 
    bool IsLoading() const { return p_state == ResourceState::Loading; }
    bool IsLoaded() const { return p_state >= ResourceState::Loaded; }
    bool HasBeenSent() const { return p_state == ResourceState::SentToGPU; }
    ResourceState GetState() const { return p_state; }

    void SetLoaded()
    {
        p_state = ResourceState::Loaded;
        EOnLoaded.Invoke();
    }

    void SetSentToGPU()
    {
        p_state = ResourceState::SentToGPU;
        EOnSentToGPU.Invoke();
    }

public:
    OnceEvent EOnLoaded;
    OnceEvent EOnSentToGPU;

protected:
    friend class ResourceManager;

    std::filesystem::path p_path;
    Core::UUID p_uuid;

    std::atomic<ResourceState> p_state{ResourceState::Unload};
};
