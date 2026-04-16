#pragma once
#include "IComponent.h"

class SceneSerializer;

class LightComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(LightComponent)
    
    void OnCreate() override;
    void OnDestroy() override;
    
    void Describe(ClassDescriptor& d) override;
    
    Vec3f GetColor() const { return m_color; }
    float GetIntensity() const { return m_intensity; }
    float GetAttenuation() const { return m_attenuation; }
    Vec3f GetOtherPosition() const { return m_otherPosition; }
    Vec2f GetAngleFactors() const { return m_angleFactors; }
    int32_t GetLightType() const { return static_cast<int32_t>(m_lightType.type); }
    
    void SetColor(const Vec4f& color) { m_color = color; }
    void SetIntensity(float intensity) { m_intensity = intensity; }
    void SetAttenuation(float attenuation) { m_attenuation = attenuation; }
    void SetOtherPosition(const Vec3f& position) { m_otherPosition = position; }
    void SetAngleFactors(const Vec2f& angleFactors) { m_angleFactors = angleFactors; }
    void SetLightType(int32_t type) { m_lightType.type = static_cast<LightType::Type>(type); }

    void SerializeData(Vec4f dataOut[4]) const;
    
private:
    friend class SceneSerializer;

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
