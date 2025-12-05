#pragma once

#include <unordered_map>
#include <vector>

#include "ComponentHandler.h"
#include "Utils/Type.h"

class IComponent;
class GameObject;
class Scene
{
public:
    Scene();
    Scene& operator=(const Scene& other) = default;
    Scene(const Scene&) = default;
    Scene(Scene&&) noexcept = default;
    virtual ~Scene();

    void OnRender();
    void OnUpdate(float deltaTime) const;

    SafePtr<GameObject> CreateGameObject(GameObject* parent = nullptr);
    SafePtr<GameObject> GetGameObject(UUID uuid) const;
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

private:
    friend GameObject;

    UUID m_rootUUID = UUID_INVALID;
    std::unordered_map<UUID, std::shared_ptr<GameObject>> m_gameObjects;
    std::unordered_map<ComponentID, std::vector<std::shared_ptr<IComponent>>> m_components;
};

template<typename T>
SafePtr<T> Scene::GetComponent(GameObject* gameObject)
{
    auto components = m_components[ComponentRegister::GetComponentID<T>()];
    for (auto& component : components)
    {
        if (component->GetGameObject() == gameObject)
            return component;
    }
    return nullptr;
}
template<typename T>
std::vector<SafePtr<T>> Scene::GetComponents(GameObject* gameObject)
{
    std::vector<SafePtr<T>> components;
    auto componentsList = m_components[ComponentRegister::GetComponentID<T>()];
    for (auto& component : componentsList)
    {
        if (component->GetGameObject() == gameObject)
            components.push_back(component);
    }
    return components;
}
template<typename T>
bool Scene::HasComponent(GameObject* gameObject)
{
    return GetComponent<T>(gameObject) != nullptr;
}

template<typename T>
SafePtr<T> Scene::AddComponent(GameObject* gameObject)
{
    auto component = std::make_shared<T>(gameObject);
    m_components[ComponentRegister::GetComponentID<T>()].push_back(component);
    return component;
}

template<typename T>
void Scene::RemoveComponent(GameObject* gameObject)
{
    auto component = GetComponent<T>(gameObject);
    if (!component)
    {
        return;
    }
    component->OnDestroy();
    m_components[ComponentRegister::GetComponentID<T>()].erase(component);
}
