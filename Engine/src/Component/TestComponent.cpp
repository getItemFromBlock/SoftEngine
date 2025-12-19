#include "TestComponent.h"

#include "TransformComponent.h"
#include "Debug/Log.h"
#include "Scene/GameObject.h"

void TestComponent::Describe(ClassDescriptor& d)
{
    d.AddFloat("Speed", m_speed);
}

void TestComponent::OnCreate()
{
    m_transform = GetGameObject()->GetComponent<TransformComponent>();
}

void TestComponent::OnUpdate(float deltaTime)
{
    if (!m_transform)
    {
        PrintError("TestComponent::OnUpdate: m_transform is null");
        return;
    }
    
    Quat rotation = m_transform->GetLocalRotation();
    m_transform->SetLocalRotation(rotation * Quat::AngleAxis(deltaTime * m_speed, Vec3f::Up()));
}
