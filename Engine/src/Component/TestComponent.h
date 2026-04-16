#pragma once
#include "IComponent.h"

class Mesh;
class TransformComponent;

class TestComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(TestComponent)
    
    void Describe(ClassDescriptor& d) override;
    
    void OnCreate() override;
    void OnGameUpdate(float deltaTime) override;

    void SetOffset(float offset) { m_offset = offset; }
    void SetSpeed(float speed) { m_speed = speed; }
    
    void AttachToCamera(bool enable) { m_attachToCamera = enable;}
private:
    float m_offset = 0.f;
    float m_speed = 1.f;
    float m_colorSpeed = 25.f;
    
    bool m_attachToCamera = false;
    
    Vec3f m_startPosition = Vec3f::Zero();
    float m_time = 0.f;
};
