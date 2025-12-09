#pragma once
#include <set>

#include "Utils/Type.h"

#include "Scene.h"

class Scene;

class GameObject
{
public:
    GameObject(Scene& scene) : m_scene(scene) {}
    GameObject& operator=(const GameObject& other) = delete;
    GameObject(const GameObject&) = delete;
    GameObject(GameObject&&) noexcept = delete;
    virtual ~GameObject() = default;
    
    template<typename T>
    SafePtr<T> GetComponent();

    template<typename T>
    SafePtr<T> AddComponent();

    template<typename T>
    bool HasComponent() const;

    template<typename T>
    void RemoveComponent() const;

    Core::UUID GetUUID() const { return m_uuid; }
    
    void SetName(std::string name) { m_name = std::move(name); }
    std::string GetName() const { return m_name; }
    
    SafePtr<GameObject> GetParent();
    
private:
    friend Scene;
    
    Core::UUID m_uuid;
    std::string m_name;
    
    Scene& m_scene;
    
    Core::UUID m_parentUUID = UUID_INVALID;
    std::set<Core::UUID> m_childrenUUID = {};
};

template<typename T>
SafePtr<T> GameObject::GetComponent() 
{
    return m_scene.GetComponent<T>(this);
}

template<typename T>
SafePtr<T> GameObject::AddComponent() 
{
    return m_scene.AddComponent<T>(this);
}

template<typename T>
bool GameObject::HasComponent() const 
{
    return m_scene.HasComponent<T>(this);
}

template<typename T>
void GameObject::RemoveComponent() const 
{
    m_scene.RemoveComponent<T>(this);
}
