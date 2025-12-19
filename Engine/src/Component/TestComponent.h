#pragma once
#include "IComponent.h"

class TransformComponent;

class TestComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(TestComponent)
    
    void Describe(ClassDescriptor& d) override;
    
    void OnCreate() override;
    void OnUpdate(float deltaTime) override;

private:
    SafePtr<TransformComponent> m_transform;
    float m_speed = 60.f;
};
