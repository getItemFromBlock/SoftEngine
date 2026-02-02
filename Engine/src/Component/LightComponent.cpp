#include "LightComponent.h"

#include "Scene/GameObject.h"

void LightComponent::OnCreate()
{
    auto lightManager = p_gameObject->GetScene()->GetLightManager();
    lightManager->AddLight(this);
}

void LightComponent::OnDestroy()
{
    IComponent::OnDestroy();
}

void LightComponent::Describe(ClassDescriptor& d)
{
    d.AddVec4f("Color", m_color);
}
