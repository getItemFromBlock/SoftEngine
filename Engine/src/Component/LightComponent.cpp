#include "LightComponent.h"

#include "Scene/GameObject.h"

void LightComponent::OnCreate()
{
    auto lightManager = p_gameObject->GetScene()->GetLightManager();
    lightManager->AddLight(this);
}

void LightComponent::OnDestroy()
{
    auto lightManager = p_gameObject->GetScene()->GetLightManager();
    lightManager->RemoveLight(this);
}

void LightComponent::Describe(ClassDescriptor& d)
{
    d.AddColor3("Color", m_color);
    d.AddFloat("Intensity", m_intensity);
}
