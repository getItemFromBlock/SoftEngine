#pragma once
#include "Core/UUID.h"

class RHIRenderer;
class GameObject;
class IComponent
{
public:
    IComponent() = default;
    IComponent(GameObject* gameObject) : m_gameObject(gameObject) {}
    IComponent& operator=(const IComponent& other) = default;
    IComponent(const IComponent&) = default;
    IComponent(IComponent&&) noexcept = default;
    virtual ~IComponent() = default;
    
    virtual void OnCreate() {}
    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender(RHIRenderer* renderer) {}
    virtual void OnDestroy() {}
    
    UUID GetUUID() const { return m_uuid; }
    GameObject* GetGameObject() const { return m_gameObject; }
private:
    UUID m_uuid;
    GameObject* m_gameObject = nullptr;
};
