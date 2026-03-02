#include "TestComponent.h"

#include "TransformComponent.h"
#include "Scene/GameObject.h"

void TestComponent::Describe(ClassDescriptor& d)
{
}

void TestComponent::OnCreate()
{
    m_startPosition = GetGameObject()->GetTransform()->GetLocalPosition();
}

void TestComponent::OnUpdate(float deltaTime)
{
    m_time += deltaTime;

    auto transform = GetGameObject()->GetTransform();

    float radius = 2.0f;

    float x = radius * cos(m_offset + m_time * m_speed);
    float y = 0;
    float z = radius * sin(m_offset + m_time * m_speed);

    transform->SetLocalPosition(m_startPosition + Vec3f(x, y, z));
}
