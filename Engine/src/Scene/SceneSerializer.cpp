#include "SceneSerializer.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "Scene.h"

#include "Component/GPUSoftBodyComponent.h"
#include "Component/IComponent.h"
#include "Component/LightComponent.h"
#include "Component/MeshComponent.h"
#include "Component/TestComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Engine.h"
#include "Debug/Log.h"
#include "Resource/CubeMap.h"
#include "Resource/Material.h"
#include "Resource/Mesh.h"
#include "Resource/ResourceManager.h"
#include "Resource/Shader.h"
#include "Resource/Texture.h"
#include "Scene/GameObject.h"
#include "Utils/File.h"

using json = nlohmann::json;

nlohmann::json SceneSerializer::ToJson(const Vec2f& value)
{
    return nlohmann::json::array({value.x, value.y});
}

nlohmann::json SceneSerializer::ToJson(const Vec3f& value)
{
    return nlohmann::json::array({value.x, value.y, value.z});
}

nlohmann::json SceneSerializer::ToJson(const Vec4f& value)
{
    return nlohmann::json::array({value.x, value.y, value.z, value.w});
}

nlohmann::json SceneSerializer::ToJson(const Vec2i& value)
{
    return nlohmann::json::array({value.x, value.y});
}

nlohmann::json SceneSerializer::ToJson(const Vec3i& value)
{
    return nlohmann::json::array({value.x, value.y, value.z});
}

nlohmann::json SceneSerializer::ToJson(const Vec4i& value)
{
    return nlohmann::json::array({value.x, value.y, value.z, value.w});
}

nlohmann::json SceneSerializer::ToJson(const Quat& value)
{
    return nlohmann::json::array({value.x, value.y, value.z, value.w});
}

bool SceneSerializer::FromJson(const nlohmann::json& value, Vec2f& out)
{
    if (!value.is_array() || value.size() != 2)
        return false;
    out = {value[0].get<float>(), value[1].get<float>()};
    return true;
}

bool SceneSerializer::FromJson(const nlohmann::json& value, Vec3f& out)
{
    if (!value.is_array() || value.size() != 3)
        return false;
    out = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
    return true;
}

bool SceneSerializer::FromJson(const nlohmann::json& value, Vec4f& out)
{
    if (!value.is_array() || value.size() != 4)
        return false;
    out = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
    return true;
}

bool SceneSerializer::FromJson(const nlohmann::json& value, Vec2i& out)
{
    if (!value.is_array() || value.size() != 2)
        return false;
    out = {value[0].get<int>(), value[1].get<int>()};
    return true;
}

bool SceneSerializer::FromJson(const nlohmann::json& value, Vec3i& out)
{
    if (!value.is_array() || value.size() != 3)
        return false;
    out = {value[0].get<int>(), value[1].get<int>(), value[2].get<int>()};
    return true;
}

bool SceneSerializer::FromJson(const nlohmann::json& value, Vec4i& out)
{
    if (!value.is_array() || value.size() != 4)
        return false;
    out = {value[0].get<int>(), value[1].get<int>(), value[2].get<int>(), value[3].get<int>()};
    return true;
}

bool SceneSerializer::FromJson(const nlohmann::json& value, Quat& out)
{
    if (!value.is_array() || value.size() != 4)
        return false;
    out = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
    return true;
}


nlohmann::json SceneSerializer::SerializeMaterial(const SafePtr<Material>& material)
{
    if (!material)
        return nullptr;

    return material->GetPath().generic_string();
}

SafePtr<Material> SceneSerializer::DeserializeMaterial(const nlohmann::json& data,
    ResourceManager* resourceManager)
{
    if (data.is_null() || !data.is_string())
        return {};

    return resourceManager->Load<Material>(data.get<std::string>());
}

nlohmann::json SceneSerializer::SerializeComponentData(const IComponent* component)
{
    return component->Serialize();
}

nlohmann::json SceneSerializer::SerializeComponent(const IComponent* component)
{
    return {
        {"type", component->GetTypeName()},
        {"uuid", static_cast<uint64_t>(component->GetUUID())},
        {"enabled", component->IsEnable()},
        {"data", SerializeComponentData(component)}
    };
}

nlohmann::json SceneSerializer::SerializeGameObject(const GameObject* object)
{
    nlohmann::json children = nlohmann::json::array();
    for (const SafePtr<GameObject>& child : object->GetChildren())
    {
        if (child)
            children.push_back(SerializeGameObject(child.getPtr()));
    }

    nlohmann::json components = nlohmann::json::array();
    for (const SafePtr<IComponent>& component : object->GetComponents())
    {
        if (component)
            components.push_back(SerializeComponent(component.getPtr()));
    }

    return {
        {"uuid", static_cast<uint64_t>(object->GetUUID())},
        {"name", object->GetName()},
        {"components", components},
        {"children", children}
    };
}

void SceneSerializer::DeserializeComponent(const nlohmann::json& data, Scene* scene, GameObject* object)
{
    const std::string type = data.value("type", "");
    const bool enabled = data.value("enabled", true);
    const nlohmann::json componentData = data.contains("data") ? data["data"] : nlohmann::json::object();

    SafePtr<IComponent> component;
    if (type == TransformComponent::GetStaticTypeName())
    {
        component = object->GetTransform();
        
        component->Deserialize(componentData);
    }
    else
    {
        const auto componentID = Engine::Get()->GetComponentRegister()->GetComponentID(type);
        if (!componentID.has_value())
        {
            PrintWarning("Unknown component type while loading scene: %s", type.c_str());
            return;
        }

        component = scene->AddComponent(object, componentID.value());
        if (!component)
            return;
        
        component->Deserialize(componentData);
    }

    if (component)
        component->SetEnable(enabled);
}

void SceneSerializer::DeserializeGameObject(const nlohmann::json& data, Scene* scene, GameObject* object)
{
    if (data.contains("name"))
        object->SetName(data["name"].get<std::string>());

    std::vector<SafePtr<IComponent>> existingComponents = object->GetComponents();
    for (const SafePtr<IComponent>& component : existingComponents)
    {
        if (!component || component->GetTypeName() == TransformComponent::GetStaticTypeName())
            continue;
        object->RemoveComponent(component->GetUUID());
    }

    if (data.contains("components") && data["components"].is_array())
    {
        for (const nlohmann::json& componentData : data["components"])
            DeserializeComponent(componentData, scene, object);
    }

    if (data.contains("children") && data["children"].is_array())
    {
        for (const nlohmann::json& childData : data["children"])
        {
            const Core::UUID childUUID = childData.contains("uuid")
                                             ? Core::UUID(childData["uuid"].get<uint64_t>())
                                             : Core::UUID();

            SafePtr<GameObject> child = scene->CreateGameObject(object, childUUID);
            if (!child)
                continue;
            DeserializeGameObject(childData, scene, child.getPtr());
        }
    }
}

bool SceneSerializer::Save(const Scene* scene, const std::filesystem::path& path)
{
    if (!scene)
        return false;

    SafePtr<GameObject> root = scene->GetRootObject();
    if (!root)
        return false;

    std::filesystem::create_directories(path.parent_path());

    json data;
    data["version"] = 1;
    data["root"] = SerializeGameObject(root.getPtr());

    return File::WriteAllText(path, data.dump(4));
}

bool SceneSerializer::Load(Scene* scene, const std::filesystem::path& path)
{
    if (!scene)
        return false;

    std::string fileContent;
    if (!File::ReadAllText(path, fileContent))
    {
        PrintError("Failed to read scene file: %s", path.generic_string().c_str());
        return false;
    }

    json data;
    try
    {
        data = json::parse(fileContent);
    }
    catch (const std::exception& exception)
    {
        PrintError("Failed to parse scene file %s: %s", path.generic_string().c_str(), exception.what());
        return false;
    }

    if (!data.contains("root") || !data["root"].is_object())
    {
        PrintError("Invalid scene file: missing root object");
        return false;
    }

    scene->Clear();

    SafePtr<GameObject> root = scene->GetRootObject();
    if (!root)
        return false;

    DeserializeGameObject(data["root"], scene, root.getPtr());
    return true;
}
