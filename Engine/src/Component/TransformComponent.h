#pragma once
#include "IComponent.h"

#include <galaxymath/Maths.h>

class TransformComponent : public IComponent
{
public:
    using IComponent::IComponent;
    
private:
    Mat4 m_modelMatrix;
    
    Vec3f m_position;
    Quat m_rotation;
    Vec3f m_scale;
};
