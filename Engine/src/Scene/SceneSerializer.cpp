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

namespace
{
    json ToJson(const Vec2f& value)
    {
        return json::array({value.x, value.y});
    }

    json ToJson(const Vec3f& value)
    {
        return json::array({value.x, value.y, value.z});
    }

    json ToJson(const Vec4f& value)
    {
        return json::array({value.x, value.y, value.z, value.w});
    }

    json ToJson(const Vec2i& value)
    {
        return json::array({value.x, value.y});
    }

    json ToJson(const Vec3i& value)
    {
        return json::array({value.x, value.y, value.z});
    }

    json ToJson(const Vec4i& value)
    {
        return json::array({value.x, value.y, value.z, value.w});
    }

    json ToJson(const Quat& value)
    {
        return json::array({value.x, value.y, value.z, value.w});
    }

    bool FromJson(const json& value, Vec2f& out)
    {
        if (!value.is_array() || value.size() != 2)
            return false;
        out = {value[0].get<float>(), value[1].get<float>()};
        return true;
    }

    bool FromJson(const json& value, Vec3f& out)
    {
        if (!value.is_array() || value.size() != 3)
            return false;
        out = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
        return true;
    }

    bool FromJson(const json& value, Vec4f& out)
    {
        if (!value.is_array() || value.size() != 4)
            return false;
        out = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
        return true;
    }

    bool FromJson(const json& value, Vec2i& out)
    {
        if (!value.is_array() || value.size() != 2)
            return false;
        out = {value[0].get<int>(), value[1].get<int>()};
        return true;
    }

    bool FromJson(const json& value, Vec3i& out)
    {
        if (!value.is_array() || value.size() != 3)
            return false;
        out = {value[0].get<int>(), value[1].get<int>(), value[2].get<int>()};
        return true;
    }

    bool FromJson(const json& value, Vec4i& out)
    {
        if (!value.is_array() || value.size() != 4)
            return false;
        out = {value[0].get<int>(), value[1].get<int>(), value[2].get<int>(), value[3].get<int>()};
        return true;
    }

    bool FromJson(const json& value, Quat& out)
    {
        if (!value.is_array() || value.size() != 4)
            return false;
        out = {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
        return true;
    }
    
    template<typename T>
    std::string SerializeResourcePath(const SafePtr<T>& resource)
    {
        if (!resource)
            return {};
        return resource->GetPath().generic_string();
    }

    template<typename T>
    SafePtr<T> LoadResource(ResourceManager* resourceManager, const json& data)
    {
        if (!data.is_string())
            return {};

        const std::string path = data.get<std::string>();
        if (path.empty())
            return {};

        return resourceManager->Load<T>(path);
    }

    json SerializeMaterial(const SafePtr<Material>& material)
    {
        if (!material)
            return nullptr;

        return material->GetPath().generic_string();
    }

    SafePtr<Material> DeserializeMaterial(const json& data, ResourceManager* resourceManager)
    {
        if (data.is_null() || !data.is_string())
            return {};

        return resourceManager->Load<Material>(data.get<std::string>());
    }

    json SerializeComponentData(const IComponent* component)
    {
        if (const auto* transform = dynamic_cast<const TransformComponent*>(component))
        {
            return {
                {"localPosition", ToJson(transform->GetLocalPosition())},
                {"localRotationEuler", ToJson(transform->GetLocalEulerAngles())},
                {"localScale", ToJson(transform->GetLocalScale())}
            };
        }

        if (const auto* mesh = dynamic_cast<const MeshComponent*>(component))
        {
            json materials = json::array();
            for (const auto& material : mesh->GetMaterials())
                materials.push_back(SerializeMaterial(material));

            return {
                {"mesh", SerializeResourcePath(mesh->GetMesh())},
                {"materials", materials},
                {"drawBounds", mesh->GetDrawBounds()}
            };
        }

        if (const auto* light = dynamic_cast<const LightComponent*>(component))
        {
            return {
                {"lightType", light->GetLightType()},
                {"color", ToJson(light->GetColor())},
                {"intensity", light->GetIntensity()},
                {"attenuation", light->GetAttenuation()},
                {"otherPosition", ToJson(light->GetOtherPosition())},
                {"angleFactors", ToJson(light->GetAngleFactors())}
            };
        }

        if (const auto* test = dynamic_cast<const TestComponent*>(component))
        {
            return {
                {"offset", test->GetOffset()},
                {"speed", test->GetSpeed()},
                {"colorSpeed", test->GetColorSpeed()},
                {"attachToCamera", test->IsAttachedToCamera()}
            };
        }
        
        if (const auto* softBody = dynamic_cast<const GPUSoftBodyComponent*>(component))
        {
            const BodySettings& settings = const_cast<GPUSoftBodyComponent*>(softBody)->GetSettings();
            return {
                {"loadedFromMesh", softBody->IsLoadedFromMesh()},
                {"initializerMesh", SerializeResourcePath(softBody->GetInitializerMesh())},
                {"drawDebug", softBody->GetDrawDebug()},
                {"settings", {
                    {"general", {
                        {"particleAmount", ToJson(settings.general.particleAmount)},
                        {"solidLayers", settings.general.solidLayers},
                        {"boneCount", ToJson(settings.general.boneCount)},
                        {"surfacePoints", ToJson(settings.general.surfacePoints)},
                        {"surfaceHeightBounds", ToJson(settings.general.surfaceHeightBounds)},
                        {"damping", settings.general.damping},
                        {"strength", settings.general.strength},
                        {"connectionStrength", settings.general.connectionStrength}
                    }},
                    {"shape", {
                        {"type", static_cast<int>(settings.shape.type)},
                        {"scale", settings.shape.scale}
                    }}
                }}
            };
        }

        return json::object();
    }

    json SerializeComponent(const IComponent* component)
    {
        return {
            {"type", component->GetTypeName()},
            {"uuid", static_cast<uint64_t>(component->GetUUID())},
            {"enabled", component->IsEnable()},
            {"data", SerializeComponentData(component)}
        };
    }

    json SerializeGameObject(const GameObject* object)
    {
        json children = json::array();
        for (const SafePtr<GameObject>& child : object->GetChildren())
        {
            if (child)
                children.push_back(SerializeGameObject(child.getPtr()));
        }

        json components = json::array();
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

    void DeserializeComponent(const json& data, Scene* scene, GameObject* object)
    {
        ResourceManager* resourceManager = Engine::Get()->GetResourceManager();
        const std::string type = data.value("type", "");
        const bool enabled = data.value("enabled", true);
        const json componentData = data.contains("data") ? data["data"] : json::object();

        SafePtr<IComponent> component;
        if (type == TransformComponent::GetStaticTypeName())
        {
            component = object->GetTransform();

            Vec3f position = object->GetTransform()->GetLocalPosition();
            Vec3f rotation = object->GetTransform()->GetLocalRotation().ToEuler();
            Vec3f scale = object->GetTransform()->GetLocalScale();

            if (componentData.contains("localPosition"))
                FromJson(componentData["localPosition"], position);
            if (componentData.contains("localRotationEuler"))
                FromJson(componentData["localRotationEuler"], rotation);
            if (componentData.contains("localScale"))
                FromJson(componentData["localScale"], scale);

            object->GetTransform()->SetLocalPosition(position);
            object->GetTransform()->SetLocalRotation(rotation);
            object->GetTransform()->SetLocalScale(scale);
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

            if (type == MeshComponent::GetStaticTypeName())
            {
                auto meshComponent = std::dynamic_pointer_cast<MeshComponent>(component.lock());
                if (!meshComponent)
                    return;
                meshComponent->SetMesh(LoadResource<Mesh>(resourceManager, componentData.contains("mesh") ? componentData["mesh"] : json()));
                meshComponent->SetDrawBounds(componentData.value("drawBounds", meshComponent->GetDrawBounds()));

                if (componentData.contains("materials") && componentData["materials"].is_array())
                {
                    size_t materialIndex = 0;
                    for (const json& materialData : componentData["materials"])
                    {
                        meshComponent->SetMaterial(materialIndex, DeserializeMaterial(materialData, resourceManager));
                        ++materialIndex;
                    }
                }
            }
            else if (type == LightComponent::GetStaticTypeName())
            {
                auto lightComponent = std::dynamic_pointer_cast<LightComponent>(component.lock());
                if (!lightComponent)
                    return;
                lightComponent->SetLightType(componentData.value("lightType", lightComponent->GetLightType()));
                Vec3f color = lightComponent->GetColor();
                if (componentData.contains("color"))
                    FromJson(componentData["color"], color);
                lightComponent->SetColor(Vec4f(color, 1.f));
                lightComponent->SetIntensity(componentData.value("intensity", lightComponent->GetIntensity()));
                lightComponent->SetAttenuation(componentData.value("attenuation", lightComponent->GetAttenuation()));
                if (componentData.contains("otherPosition"))
                {
                    Vec3f otherPosition = lightComponent->GetOtherPosition();
                    if (FromJson(componentData["otherPosition"], otherPosition))
                        lightComponent->SetOtherPosition(otherPosition);
                }
                if (componentData.contains("angleFactors"))
                {
                    Vec2f angleFactors = lightComponent->GetAngleFactors();
                    if (FromJson(componentData["angleFactors"], angleFactors))
                        lightComponent->SetAngleFactors(angleFactors);
                }
            }
            else if (type == TestComponent::GetStaticTypeName())
            {
                auto testComponent = std::dynamic_pointer_cast<TestComponent>(component.lock());
                if (!testComponent)
                    return;
                testComponent->SetOffset(componentData.value("offset", testComponent->GetOffset()));
                testComponent->SetSpeed(componentData.value("speed", testComponent->GetSpeed()));
                testComponent->SetColorSpeed(componentData.value("colorSpeed", testComponent->GetColorSpeed()));
                testComponent->AttachToCamera(componentData.value("attachToCamera", testComponent->IsAttachedToCamera()));
            }
            else if (type == GPUSoftBodyComponent::GetStaticTypeName())
            {
                auto softBodyComponent = std::dynamic_pointer_cast<GPUSoftBodyComponent>(component.lock());
                if (!softBodyComponent)
                    return;
                BodySettings& settings = softBodyComponent->GetSettings();
                const json settingsData = componentData.contains("settings") ? componentData["settings"] : json::object();

                if (settingsData.contains("general"))
                {
                    const json& general = settingsData["general"];
                    if (general.contains("particleAmount"))
                        FromJson(general["particleAmount"], settings.general.particleAmount);
                    settings.general.solidLayers = general.value("solidLayers", settings.general.solidLayers);
                    if (general.contains("boneCount"))
                        FromJson(general["boneCount"], settings.general.boneCount);
                    if (general.contains("surfacePoints"))
                        FromJson(general["surfacePoints"], settings.general.surfacePoints);
                    if (general.contains("surfaceHeightBounds"))
                        FromJson(general["surfaceHeightBounds"], settings.general.surfaceHeightBounds);
                    settings.general.damping = general.value("damping", settings.general.damping);
                    settings.general.strength = general.value("strength", settings.general.strength);
                    settings.general.connectionStrength = general.value("connectionStrength", settings.general.connectionStrength);
                }
                if (settingsData.contains("shape"))
                {
                    const json& shape = settingsData["shape"];
                    settings.shape.type = static_cast<BodySettings::Shape::Type>(shape.value("type", static_cast<int>(settings.shape.type)));
                    settings.shape.scale = shape.value("scale", settings.shape.scale);
                }

                softBodyComponent->SetDrawDebug(componentData.value("drawDebug", softBodyComponent->GetDrawDebug()));
                SafePtr<Mesh> initializerMesh = LoadResource<Mesh>(
                    resourceManager,
                    componentData.contains("initializerMesh") ? componentData["initializerMesh"] : json());

                if (componentData.value("loadedFromMesh", false) && initializerMesh)
                    softBodyComponent->CreateFromMesh(initializerMesh);
                else
                    softBodyComponent->ApplySettings();
            }
        }

        if (component)
            component->SetEnable(enabled);
    }

    void DeserializeGameObject(const json& data, Scene* scene, GameObject* object)
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
            for (const json& componentData : data["components"])
                DeserializeComponent(componentData, scene, object);
        }

        if (data.contains("children") && data["children"].is_array())
        {
            for (const json& childData : data["children"])
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
