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
    void OnUpdate(float deltaTime) override;

private:
    bool m_bool = false;
    float m_float = 60.f;
    int m_int = 0;
    Vec2f m_vec2f;
    Vec3f m_vec3f;
    Vec4f m_vec4f;
    Quat m_quat;
    Vec3f m_color3;
    Vec4f m_color4;
    SafePtr<Texture> m_texture;
    SafePtr<CubeMap> m_cubeMap;
    SafePtr<Mesh> m_mesh;
    
};
