#include "TestComponent.h"

#include "TransformComponent.h"
#include "Core/Engine.h"
#include "Scene/GameObject.h"

void TestComponent::Describe(ClassDescriptor& d)
{
    d.AddBool("Attach to camera", m_attachToCamera);
}

void TestComponent::OnCreate()
{
    m_startPosition = GetGameObject()->GetTransform()->GetLocalPosition();
}

void TestComponent::OnUpdate(float deltaTime)
{
    auto transform = GetGameObject()->GetTransform();
    if (m_attachToCamera)
    {
        auto position = p_gameObject->GetScene()->GetEditorCamera()->GetTransform()->GetLocalPosition();
        
        transform->SetWorldPosition(position);
        return;
    }
    m_time += deltaTime;


    float radius = 2.0f;

    float x = radius * cos(m_offset + m_time * m_speed);
    float y = 0;
    float z = radius * sin(m_offset + m_time * m_speed);

    transform->SetLocalPosition(m_startPosition + Vec3f(x, y, z));
}
