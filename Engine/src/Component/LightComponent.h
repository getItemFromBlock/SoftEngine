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

    void SerializeData(Vec4f dataOut[4]) const;
    
private:
    Vec3f m_color = Vec3f::One();
    float m_intensity = 1.0f;
    float m_attenuation = 10.0f;
    Vec3f m_otherPosition = Vec3f::Zero();
    Vec2f m_angleFactors = Vec2f(45.0f,40.0f);

    struct LightType
    {
        enum Type : int32_t
        {
            Directional,
            Point,
            Spot,
            Line
        } type = Point;

        static const char *to_cstr()
        {
            return "Directional\0Point\0Spot\0Line\0";
        }
    } m_lightType;
};
