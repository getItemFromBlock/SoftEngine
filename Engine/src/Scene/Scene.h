#pragma once
#include <unordered_map>
#include <vector>
#include <shared_mutex>
#include <mutex>

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

using GameObjectList = std::unordered_map<Core::UUID, std::shared_ptr<GameObject>>;
class Scene
{
public:
    Scene();
    Scene& operator=(const Scene& other) = delete;
    Scene(const Scene&) = delete;
    Scene(Scene&&) noexcept = delete;
    virtual ~Scene();

    void OnRender(RHIRenderer* renderer);
    void OnUpdate(float deltaTime);

    const GameObjectList& GetGameObjects() const { return m_gameObjects; }
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
    std::vector<SafePtr<IComponent>> GetComponents(const GameObject* gameObject);

    template<typename T>
    bool HasComponent(GameObject* gameObject);

    template<typename T>
    SafePtr<T> AddComponent(GameObject* gameObject);

    template<typename T>
    void RemoveComponent(GameObject* gameObject);
    
    void RemoveAllComponents(GameObject* gameObject);
#pragma endregion 

    const CameraData& GetCameraData() const { return m_cameraData; }
    
private:
    void UpdateCamera(float deltaTime) const;
private:
    friend GameObject;

    Core::UUID m_rootUUID = UUID_INVALID;
    GameObjectList m_gameObjects;
    std::unordered_map<ComponentID, std::vector<std::shared_ptr<IComponent>>> m_components;
    
    CameraData m_cameraData;
    
    // Use recursive_mutex to allow same thread to lock multiple times
    mutable std::recursive_mutex m_gameObjectsMutex;
    mutable std::recursive_mutex m_componentsMutex;
};

template<typename T>
SafePtr<T> Scene::GetComponent(GameObject* gameObject)
{
    std::scoped_lock lock(m_componentsMutex);
    
    auto it = m_components.find(ComponentRegister::GetComponentID<T>());
    if (it == m_components.end()) return {};

    for (const auto& component : it->second) {
        if (component->GetGameObject() == gameObject) {
            return SafePtr<T>(std::static_pointer_cast<T>(component));
        }
    }
    return {};
}

template<typename T>
std::vector<SafePtr<T>> Scene::GetComponents(GameObject* gameObject)
{
    static_assert(std::is_base_of_v<IComponent, T>, "T must inherit from IComponent");

    std::scoped_lock lock(m_componentsMutex);
    
    std::vector<SafePtr<T>> out;
    auto it = m_components.find(ComponentRegister::GetComponentID<T>());
    if (it == m_components.end()) return out;

    for (const auto& component : it->second)
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

    std::scoped_lock lock(m_componentsMutex);
    m_components[ComponentRegister::GetComponentID<T>()].push_back(component);
    
    return component;
}

template<typename T>
void Scene::RemoveComponent(GameObject* gameObject)
{
    static_assert(std::is_base_of_v<IComponent, T>, "T must inherit from IComponent");

    std::scoped_lock lock(m_componentsMutex);
    
    auto it = m_components.find(ComponentRegister::GetComponentID<T>());
    if (it == m_components.end()) return;
    
    auto& componentList = it->second;
    auto removeIt = std::find_if(componentList.begin(), componentList.end(),
        [gameObject](const std::shared_ptr<IComponent>& comp) {
            return comp->GetGameObject() == gameObject;
        });
    
    if (removeIt != componentList.end())
    {
        (*removeIt)->OnDestroy();
        componentList.erase(removeIt);
    }
}