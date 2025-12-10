#pragma once
#include <unordered_map>
#include <vector>
#include <galaxymath/Maths.h>

#include "ComponentHandler.h"
#include "Utils/Type.h"

class TransformComponent;
class RHIRenderer;
class IComponent;
class GameObject;

struct CameraData
{
    std::unique_ptr<TransformComponent> transform;
    
    Mat4 VP;
};

class Scene
{
public:
    Scene();
    Scene& operator=(const Scene& other) = default;
    Scene(const Scene&) = default;
    Scene(Scene&&) noexcept = default;
    virtual ~Scene();

    void OnRender(RHIRenderer* renderer);
    void OnUpdate(float deltaTime);

    SafePtr<GameObject> CreateGameObject(GameObject* parent = nullptr);
    SafePtr<GameObject> GetGameObject(Core::UUID UUID) const;
    SafePtr<GameObject> GetRootObject() const;
    void DestroyGameObject(GameObject* gameObject);
    
    void SetParent(GameObject* object, GameObject* parent);
    void RemoveChild(GameObject* object, GameObject* child);

    #pragma region Component
    template<typename T>
    SafePtr<T> GetComponent(GameObject* gameObject);

    template<typename T>
    std::vector<SafePtr<T>> GetComponents(GameObject* gameObject);

    template<typename T>
    bool HasComponent(GameObject* gameObject);

    template<typename T>
    SafePtr<T> AddComponent(GameObject* gameObject);

    template<typename T>
    void RemoveComponent(GameObject* gameObject);
    
    void RemoveAllComponents(GameObject* gameObject);
#pragma endregion 

    const CameraData& GetCameraData() const { return m_cameraData; }
    void UpdateCamera(float deltaTime) const;
private:
    friend GameObject;

    Core::UUID m_rootUUID = UUID_INVALID;
    std::unordered_map<Core::UUID, std::shared_ptr<GameObject>> m_gameObjects;
    std::unordered_map<ComponentID, std::vector<std::shared_ptr<IComponent>>> m_components;
    
    CameraData m_cameraData;
};

template<typename T>
SafePtr<T> Scene::GetComponent(GameObject* gameObject)
{
    static_assert(std::is_base_of_v<IComponent, T>, "T must inherit from IComponent");

    auto& components = m_components[ComponentRegister::GetComponentID<T>()];

    for (const std::shared_ptr<IComponent>& component : components)
    {
        if (component->GetGameObject() == gameObject)
        {
            return SafePtr<T>(std::static_pointer_cast<T>(component));
        }
    }
    return {};
}

template<typename T>
std::vector<SafePtr<T>> Scene::GetComponents(GameObject* gameObject)
{
    static_assert(std::is_base_of_v<IComponent, T>, "T must inherit from IComponent");

    std::vector<SafePtr<T>> out;
    auto& componentsList = m_components[ComponentRegister::GetComponentID<T>()];

    for (const auto& component : componentsList)
    {
        if (component->GetGameObject() == gameObject)
        {
            out.push_back(SafePtr<T>(std::static_pointer_cast<T>(component)));
        }
    }
    return out;
}

template<typename T>
bool Scene::HasComponent(GameObject* gameObject)
{
    static_assert(std::is_base_of_v<IComponent, T>, "T must inherit from IComponent");
    return GetComponent<T>(gameObject) != nullptr;
}

template<typename T>
SafePtr<T> Scene::AddComponent(GameObject* gameObject)
{
    static_assert(std::is_base_of_v<IComponent, T>, "T must inherit from IComponent");
    auto component = std::make_shared<T>(gameObject);
    m_components[ComponentRegister::GetComponentID<T>()].push_back(component);
    return component;
}

template<typename T>
void Scene::RemoveComponent(GameObject* gameObject)
{
    static_assert(std::is_base_of_v<IComponent, T>, "T must inherit from IComponent");
    auto component = GetComponent<T>(gameObject);
    if (!component)
    {
        return;
    }
    component->OnDestroy();
    m_components[ComponentRegister::GetComponentID<T>()].erase(component);
}
