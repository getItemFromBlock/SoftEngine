#pragma once
#include <set>

#include "Utils/Type.h"

#include "Component/TransformComponent.h"

#include "Scene.h"

class Scene;

class GameObject
{
public:
    GameObject(Scene& scene);
    GameObject& operator=(const GameObject& other) = delete;
    GameObject(const GameObject&) = delete;
    GameObject(GameObject&&) noexcept = delete;
    virtual ~GameObject();
    
    template<typename T>
    SafePtr<T> GetComponent();
    
    SafePtr<TransformComponent> GetTransform() const { return m_transform; }

    template<typename T>
    SafePtr<T> AddComponent();
    
    std::vector<SafePtr<IComponent>> GetComponents() const;

    template<typename T>
    bool HasComponent() const;

    template<typename T>
    void RemoveComponent();
    void RemoveComponent(Core::UUID compId) const;

    Core::UUID GetUUID() const { return m_uuid; }
    
    void SetName(const std::string& name) { m_name = name; }
    std::string GetName() const { return m_name; }
    
    bool HasParent() const;
    SafePtr<GameObject> GetParent() const;
    
    std::vector<SafePtr<GameObject>> GetChildren() const;
    std::set<Core::UUID> GetChildrenUUID() const { return m_childrenUUID; }
    
    Scene* GetScene() const { return &m_scene; }
private:
    friend Scene;
    
    Core::UUID m_uuid;
    std::string m_name;
    
    Scene& m_scene;
    
    Core::UUID m_parentUUID = UUID_INVALID;
    std::set<Core::UUID> m_childrenUUID = {};
    
    SafePtr<TransformComponent> m_transform = {};
};

template<typename T>
SafePtr<T> GameObject::GetComponent() 
{
    if (std::is_same_v<T, TransformComponent>) // Sometimes break with compiler
        return m_transform;
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
void GameObject::RemoveComponent()
{
    m_scene.RemoveComponent<T>(this);
}
