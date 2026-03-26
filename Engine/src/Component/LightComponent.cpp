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
    d.AddFloat("Attenuation", m_attenuation);
    d.AddVec2f("Angles", m_angleFactors);
}

void LightComponent::SerializeData(Vec4f dataOut[4]) const
{
    dataOut[0] = Vec4f(GetGameObject()->GetTransform()->GetWorldPosition(), 1.0f);
    dataOut[1] = Vec4f(GetGameObject()->GetTransform()->GetForward());
    float cosOuterAngle = cosf(DegToRad * m_angleFactors.x);
    float cosInnerAngle = cosf(DegToRad * m_angleFactors.y);
    float invAngleRange = 1.0f / (cosInnerAngle - cosOuterAngle);
    dataOut[2] = Vec4f(invAngleRange, -cosOuterAngle * invAngleRange, m_attenuation, 0);
    dataOut[3] = Vec4f(GetColor(), GetIntensity());
}
