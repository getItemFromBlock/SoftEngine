#include "TestComponent.h"

#include "TransformComponent.h"

void TestComponent::Describe(ClassDescriptor& d)
{
    d.AddBool("bool", m_bool);
    d.AddFloat("float", m_float);
    d.AddInt("int", m_int);
    d.AddVec2f("vec2f", m_vec2f);
    d.AddVec3f("vec3f", m_vec3f);
    d.AddVec4f("vec4f", m_vec4f);
    d.AddQuat("quat", m_quat);
    d.AddTexture("texture", m_texture);
    d.AddCubeMap("cubeMap", m_cubeMap);
    d.AddColor3("color3", m_color3);
    d.AddColor4("color4", m_color4);
    d.AddMesh("mesh", m_mesh);
}

void TestComponent::OnCreate()
{
}

void TestComponent::OnUpdate(float deltaTime)
{
}
