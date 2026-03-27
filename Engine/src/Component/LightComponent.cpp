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
    d.AddEnum("Light Type", reinterpret_cast<int32_t*>(&m_lightType.type), m_lightType.to_cstr());
    d.AddColor3("Color", m_color);
    d.AddFloat("Intensity", m_intensity);
    if (m_lightType.type != LightType::Directional)
        d.AddFloat("Attenuation", m_attenuation);
    if (m_lightType.type == LightType::Spot)
        d.AddVec2f("Angles", m_angleFactors);
    else if (m_lightType.type == LightType::Line)
        d.AddVec3f("Delta Position", m_otherPosition);
}

void LightComponent::SerializeData(Vec4f dataOut[4]) const
{
    if (m_lightType.type == LightType::Directional)
        dataOut[0] = Vec4f(-GetGameObject()->GetTransform()->GetForward(), 0.0f);
    else
        dataOut[0] = Vec4f(GetGameObject()->GetTransform()->GetWorldPosition(), 1.0f);

    if (m_lightType.type == LightType::Line)
        dataOut[1] = Vec4f(m_otherPosition, 1.0f);
    else
        dataOut[1] = Vec4f(-GetGameObject()->GetTransform()->GetForward(), 0.0f);

    if (m_lightType.type == LightType::Spot)
    {
        float cosOuterAngle = cosf(DegToRad * m_angleFactors.x);
        float cosInnerAngle = cosf(DegToRad * m_angleFactors.y);
        float invAngleRange = 1.0f / (cosInnerAngle - cosOuterAngle);
        dataOut[2] = Vec4f(invAngleRange, -cosOuterAngle * invAngleRange, 1 / (m_attenuation * m_attenuation), 0);
    }
    else if (m_lightType.type != LightType::Directional)
        dataOut[2] = Vec4f(0, 1, 1 / (m_attenuation * m_attenuation), 0);
    else
        dataOut[2] = Vec4f(0, 1, 0, 0);

    dataOut[3] = Vec4f(GetColor(), GetIntensity());
}
