#pragma once
#include "Core/UUID.h"

class RHIRenderer;
class GameObject;
class IComponent
{
public:
    IComponent() = default;
    IComponent(GameObject* gameObject) : p_gameObject(gameObject) {}
    IComponent& operator=(const IComponent& other) = default;
    IComponent(const IComponent&) = default;
    IComponent(IComponent&&) noexcept = default;
    virtual ~IComponent() = default;
    
    virtual void OnCreate() {}
    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender(RHIRenderer* renderer) {}
    virtual void OnDestroy() {}
    
    Core::UUID GetUUID() const { return p_uuid; }
    GameObject* GetGameObject() const { return p_gameObject; }
protected:
    Core::UUID p_uuid;
    GameObject* p_gameObject = nullptr;
};
