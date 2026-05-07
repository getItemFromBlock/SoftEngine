#pragma once

#include <filesystem>
#include <galaxymath/Maths.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "Resource/ResourceManager.h"
#include "Utils/Type.h"

class GameObject;
class IComponent;
class Material;
class ResourceManager;
class Scene;

class SceneSerializer
{
public:

    static nlohmann::json ToJson(const Vec2f& value);
    static nlohmann::json ToJson(const Vec3f& value);
    static nlohmann::json ToJson(const Vec4f& value);
    static nlohmann::json ToJson(const Vec2i& value);
    static nlohmann::json ToJson(const Vec3i& value);
    static nlohmann::json ToJson(const Vec4i& value);
    static nlohmann::json ToJson(const Quat& value);

    static bool FromJson(const nlohmann::json& value, Vec2f& out);
    static bool FromJson(const nlohmann::json& value, Vec3f& out);
    static bool FromJson(const nlohmann::json& value, Vec4f& out);
    static bool FromJson(const nlohmann::json& value, Vec2i& out);
    static bool FromJson(const nlohmann::json& value, Vec3i& out);
    static bool FromJson(const nlohmann::json& value, Vec4i& out);
    static bool FromJson(const nlohmann::json& value, Quat& out);
    
    template<typename T>
    static std::string SerializeResourcePath(const SafePtr<T>& resource);
    template<typename T>
    static SafePtr<T> LoadResource(ResourceManager* resourceManager, const nlohmann::json& data);

    static nlohmann::json SerializeMaterial(const SafePtr<Material>& material);
    static SafePtr<Material> DeserializeMaterial(const nlohmann::json& data, ResourceManager* resourceManager);

    static nlohmann::json SerializeComponentData(const IComponent* component);
    static nlohmann::json SerializeComponent(const IComponent* component);
    static nlohmann::json SerializeGameObject(const GameObject* object);

    static void DeserializeComponent(const nlohmann::json& data, Scene* scene, GameObject* object);
    static void DeserializeGameObject(const nlohmann::json& data, Scene* scene, GameObject* object);

    static bool Save(const Scene* scene, const std::filesystem::path& path);
    static bool Load(Scene* scene, const std::filesystem::path& path);
};

template <typename T>
std::string SceneSerializer::SerializeResourcePath(const SafePtr<T>& resource)
{
    if (!resource)
        return {};
    return resource->GetPath().generic_string();
}

template <typename T>
SafePtr<T> SceneSerializer::LoadResource(ResourceManager* resourceManager, const nlohmann::json& data)
{
    if (!data.is_string())
        return {};

    const std::string path = data.get<std::string>();
    if (path.empty())
        return {};

    return resourceManager->Load<T>(path);
}
