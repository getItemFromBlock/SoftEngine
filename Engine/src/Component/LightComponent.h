#pragma once
#include "IComponent.h"

class LightComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(LightComponent)
    
    void OnCreate() override;
    void OnDestroy() override;
    
    void Describe(ClassDescriptor& d) override;
    
    Vec4f GetColor() const { return m_color; }
private:
    Vec4f m_color = Vec4f::One();
};
