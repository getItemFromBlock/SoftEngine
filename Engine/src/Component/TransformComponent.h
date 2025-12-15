#pragma once
#include "IComponent.h"

#include <galaxymath/Maths.h>

#include "Utils/Event.h"

enum class Space
{
    World,
    Local,
};

class TransformComponent : public IComponent
{
public:
    DECLARE_COMPONENT_TYPE(TransformComponent)
    
    void OnUpdate(float deltaTime) override;
    
    const Mat4& GetModelMatrix() const { return m_modelMatrix; }
    
    Vec3f GetForward() const;
    Vec3f GetRight() const;
    Vec3f GetUp() const;

    void SetLocalPosition(const Vec3f& position);
    Vec3f GetLocalPosition() const { return m_localPosition; }
    
    void SetWorldPosition(const Vec3f& position);
    Vec3f GetWorldPosition() const;
    
    void SetLocalRotation(const Quat& rotation);
    Quat GetLocalRotation() const { return m_localRotation; }
    
    void SetWorldRotation(const Quat& rotation);
    Quat GetWorldRotation() const;
    
    void SetLocalScale(const Vec3f& scale);
    Vec3f GetLocalScale() const { return m_localScale; }
    
    void SetWorldScale(const Vec3f& scale);
    Vec3f GetWorldScale() const;
    
    void Rotate(const Vec3f& axis, const float angle, Space relativeTo = Space::Local);
    void RotateAround(const Vec3f point, const Vec3f axis, const float angle);
    void RotateAround(const Vec3f axis, const float angle);
    Vec3f TransformDirection(Vec3f dir) const;
public:
    Event<> EOnUpdateModelMatrix;
private:
    void SetDirty() { m_dirty = true; }

    void ComputeModelMatrix();
private:
    Mat4 m_modelMatrix;
    
    Vec3f m_localPosition = Vec3f::Zero();
    Quat m_localRotation = Quat::Identity();
    Vec3f m_localScale = Vec3f::One();
    
    bool m_dirty = true;
};
