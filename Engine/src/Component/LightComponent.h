#pragma once
#include "IComponent.h"

class LightComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(LightComponent)
    
    void OnCreate() override;
    void OnDestroy() override;
    
    void Describe(ClassDescriptor& d) override;
    
    Vec3f GetColor() const { return m_color; }
    float GetIntensity() const { return m_intensity; }
    
    void SetColor(const Vec4f& color) { m_color = color; }
    void SetIntensity(float intensity) { m_intensity = intensity; }
    
private:
    Vec3f m_color = Vec3f::One();
    float m_intensity = 1.0f;
};
