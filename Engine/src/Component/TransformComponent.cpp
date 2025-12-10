#include "TransformComponent.h"

void TransformComponent::OnUpdate(float deltaTime)
{
    if (m_dirty)
    {
        ComputeModelMatrix();
    }
}

Vec3f TransformComponent::GetForward() const
{
    return m_localRotation * Vec3f::Forward();
}

Vec3f TransformComponent::GetRight() const
{
    return m_localRotation * Vec3f::Right();
}

Vec3f TransformComponent::GetUp() const
{
    return m_localRotation * Vec3f::Up();
}

void TransformComponent::SetLocalPosition(const Vec3f& position)
{
    m_localPosition = position;
    SetDirty();
}

void TransformComponent::SetWorldPosition(const Vec3f& position)
{
    //TODO
    SetLocalPosition(position);
}

Vec3f TransformComponent::GetWorldPosition() const
{
    //TODO
    return GetLocalPosition();
}

void TransformComponent::SetLocalRotation(const Quat& rotation)
{
    m_localRotation = rotation;
    SetDirty();
}

void TransformComponent::SetWorldRotation(const Quat& rotation)
{
    //TODO:
    SetLocalRotation(rotation);
}

Quat TransformComponent::GetWorldRotation() const
{
    //TODO
    return GetLocalRotation();
}

void TransformComponent::SetLocalScale(const Vec3f& scale)
{
    m_localScale = scale;
    SetDirty();
}

void TransformComponent::SetWorldScale(const Vec3f& scale)
{    
    //TODO
    SetLocalScale(scale);
}

Vec3f TransformComponent::GetWorldScale() const
{
    //TODO
    return GetLocalScale(); 
}

void TransformComponent::Rotate(const Vec3f& axis, const float angle, Space relativeTo)
{
    if (relativeTo == Space::Local)
        RotateAround(TransformDirection(axis), angle);
    else
        RotateAround(axis, angle);
}

void TransformComponent::RotateAround(const Vec3f point, const Vec3f axis, const float angle)
{
    const Quat q = Quat::AngleAxis(angle, axis);
    Vec3f dif = GetWorldPosition() - point;
    dif = q * dif;
    SetWorldPosition(point + dif);
    const Quat worldRotation = GetWorldRotation();
    SetWorldRotation(worldRotation * worldRotation.GetInverse() * q * worldRotation);
}

void TransformComponent::RotateAround(const Vec3f axis, const float angle)
{
    if (angle == 0.0f)
        return;
    const Quat rotation = Quat::AngleAxis(angle, axis);

    const Vec3f pivot = GetWorldPosition();
    Vec3f relativePosition = Vec3f::Zero();

    relativePosition = rotation * relativePosition;

    SetWorldPosition(pivot + relativePosition);

    const Quat worldRotation = GetWorldRotation();
    const Quat rot = worldRotation * worldRotation.GetInverse() * rotation * worldRotation;
    SetWorldRotation(rot);
}

Vec3f TransformComponent::TransformDirection(Vec3f dir) const
{
	return GetWorldRotation() * dir;
}

void TransformComponent::ComputeModelMatrix()
{
    m_modelMatrix = Mat4::CreateTransformMatrix(m_localPosition, m_localRotation, m_localScale);
    EOnUpdateModelMatrix.Invoke();
    m_dirty = false;
}
