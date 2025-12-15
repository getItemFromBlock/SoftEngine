#pragma once
#include "Core/UUID.h"

class RHIRenderer;
class GameObject;

#define DECLARE_COMPONENT_TYPE(T) \
    T() = default; \
    T(GameObject* gameObject) : IComponent(gameObject) {} \
    T& operator=(const T& other) = default;\
    T(const T&) = default;\
    T(T&&) noexcept = default;\
    virtual ~T() override = default;\
    std::string GetTypeName() const override { return #T; } 
    

class IComponent
{
public:
    IComponent() = default;
    IComponent(GameObject* gameObject) : p_gameObject(gameObject) {}
    IComponent& operator=(const IComponent& other) = default;
    IComponent(const IComponent&) = default;
    IComponent(IComponent&&) noexcept = default;
    virtual ~IComponent() = default;
    
    virtual std::string GetTypeName() const { return "IComponent"; }
    
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
