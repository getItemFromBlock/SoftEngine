#pragma once
#include <galaxymath/Maths.h>

#include "Core/UUID.h"
#include "Scene/ClassDescriptor.h"

#define DECLARE_COMPONENT_TYPE_PARENT(T, P) \
    T() = default; \
    T(GameObject* gameObject) : P(gameObject) {} \
    T& operator=(const T& other) = default;\
    T(const T&) = default;\
    T(T&&) noexcept = default;\
    virtual ~T() override = default;\
    const char* GetTypeName() const override { return #T; } \
    using Super = P;


#define DECLARE_COMPONENT_TYPE(T) DECLARE_COMPONENT_TYPE_PARENT(T, IComponent)
    

class VulkanRenderer;
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
    
    virtual const char* GetTypeName() const { return "IComponent"; }
    virtual void Describe(ClassDescriptor& d) {}

    virtual void OnCreate() {}
    virtual void OnStart() {}
    virtual void OnUpdate(float deltaTime) {}
    virtual void OnRender(VulkanRenderer* renderer) {}
    virtual void OnDestroy() {}
    
    bool IsEnable() const { return p_enable; }
    void SetEnable(bool enable) { p_enable = enable; }
    
    Core::UUID GetUUID() const { return p_uuid; }
    GameObject* GetGameObject() const { return p_gameObject; }
protected:
    bool p_enable = true;
    Core::UUID p_uuid;
    GameObject* p_gameObject = nullptr;
};
