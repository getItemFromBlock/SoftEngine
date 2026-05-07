#include "TestComponent.h"

#include <nlohmann/json.hpp>

#include "LightComponent.h"
#include "MeshComponent.h"
#include "TransformComponent.h"
#include "Core/Engine.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"

void TestComponent::Describe(ClassDescriptor& d)
{
    d.AddBool("Attach to camera", m_attachToCamera);
}

void TestComponent::OnCreate()
{
    m_startPosition = GetGameObject()->GetTransform()->GetLocalPosition();
}

void TestComponent::OnGameUpdate(float deltaTime)
{
    auto transform = GetGameObject()->GetTransform();
    if (m_attachToCamera)
    {
        Camera* editorCamera = p_gameObject->GetScene()->GetEditorCamera();
        auto position = editorCamera->GetTransform()->GetLocalPosition();
        transform->SetWorldPosition(position);
        return;
    }
    
    m_time += deltaTime;
    
    float radius = 2.0f;
    
    float x = radius * cos(m_offset + m_time * m_speed);
    float y = 0;
    float z = radius * sin(m_offset + m_time * m_speed);
    
    transform->SetLocalPosition(m_startPosition + Vec3f(x, y, z));
    
    if (auto lightComponent = p_gameObject->GetComponent<LightComponent>())
    {
        Vec3f color = Color::FromHSV(std::fmodf(m_offset + m_time * m_colorSpeed, 360.f), 1.f, 1.f);  
        lightComponent->SetColor(color);
        if (auto meshComponent = p_gameObject->GetComponent<MeshComponent>())
        {
            auto mat = meshComponent->GetMaterial(0);
            mat->SetAttribute("material.color", Vec4f(color, 1.f));
        }
    }
}

nlohmann::json TestComponent::Serialize() const
{
    return {
        {"offset", GetOffset()},
        {"speed", GetSpeed()},
        {"colorSpeed", GetColorSpeed()},
        {"attachToCamera", IsAttachedToCamera()}
    };
}

void TestComponent::Deserialize(const nlohmann::json& json)
{
    SetOffset(json.value("offset", GetOffset()));
    SetSpeed(json.value("speed", GetSpeed()));
    SetColorSpeed(json.value("colorSpeed", GetColorSpeed()));
    AttachToCamera(json.value("attachToCamera", IsAttachedToCamera()));
}
