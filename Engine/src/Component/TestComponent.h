#pragma once
#include "IComponent.h"

class Mesh;
class TransformComponent;
class SceneSerializer;

class TestComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(TestComponent)
    
    void Describe(ClassDescriptor& d) override;
    
    void OnCreate() override;
    void OnGameUpdate(float deltaTime) override;

    float GetOffset() const { return m_offset; }
    void SetOffset(float offset) { m_offset = offset; }
    float GetSpeed() const { return m_speed; }
    void SetSpeed(float speed) { m_speed = speed; }
    float GetColorSpeed() const { return m_colorSpeed; }
    void SetColorSpeed(float colorSpeed) { m_colorSpeed = colorSpeed; }
    
    bool IsAttachedToCamera() const { return m_attachToCamera; }
    void AttachToCamera(bool enable) { m_attachToCamera = enable;}
private:
    friend class SceneSerializer;

    float m_offset = 0.f;
    float m_speed = 1.f;
    float m_colorSpeed = 25.f;
    
    bool m_attachToCamera = false;
    
    Vec3f m_startPosition = Vec3f::Zero();
    float m_time = 0.f;
};
