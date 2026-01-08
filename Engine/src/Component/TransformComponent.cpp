#include "TransformComponent.h"
#include "Scene/GameObject.h"

void TransformComponent::Describe(ClassDescriptor& d)
{
    d.AddProperty("", PropertyType::Transform, this);
}

void TransformComponent::OnUpdate(float deltaTime)
{
    UpdateMatrix();
}

Mat4 TransformComponent::GetWorldMatrix() const
{
    return m_modelMatrix;
}

Mat4 TransformComponent::GetLocalMatrix() const
{
    return Mat4::CreateTransformMatrix(m_localPosition, m_localRotation, m_localScale);
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
    if (!p_gameObject)
    {
        SetLocalPosition(position);
        return;
    }

    auto parent = p_gameObject->GetParent();
    if (parent)
    {
        Mat4 parentWorldMatrix = parent->GetTransform()->GetWorldMatrix();
        SetLocalPosition(parentWorldMatrix.GetInverseMatrix() * position);
    }
    else
    {
        SetLocalPosition(position);
    }
}

Vec3f TransformComponent::GetWorldPosition() const
{
    if (!p_gameObject)
        return m_localPosition;
    return GetWorldMatrix().GetTranslation();
}

void TransformComponent::SetLocalRotation(const Quat& rotation)
{
    m_localRotation = rotation;
    SetDirty();
}

void TransformComponent::SetWorldRotation(const Quat& rotation)
{
    if (!p_gameObject)
    {
        SetLocalRotation(rotation);
        return;
    }

    auto parent = p_gameObject->GetParent();
    if (parent)
    {
        Quat parentRotation = parent->GetTransform()->GetWorldRotation();
        SetLocalRotation(parentRotation.GetInverse() * rotation);
    }
    else
    {
        SetLocalRotation(rotation);
    }
}

Quat TransformComponent::GetWorldRotation() const
{
    if (!p_gameObject)
        return m_localRotation;

    auto parent = p_gameObject->GetParent();
    if (parent)
    {
        return parent->GetTransform()->GetWorldRotation() * m_localRotation;
    }
    return m_localRotation;
}

void TransformComponent::SetLocalScale(const Vec3f& scale)
{
    m_localScale = scale;
    SetDirty();
}

void TransformComponent::SetWorldScale(const Vec3f& scale)
{
    if (!p_gameObject)
    {
        SetLocalScale(scale);
        return;
    }

    auto parent = p_gameObject->GetParent();
    if (parent)
    {
        Vec3f parentScale = parent->GetTransform()->GetWorldScale();

        Vec3f newLocalScale;
        newLocalScale.x = (parentScale.x != 0.0f) ? scale.x / parentScale.x : 0.0f;
        newLocalScale.y = (parentScale.y != 0.0f) ? scale.y / parentScale.y : 0.0f;
        newLocalScale.z = (parentScale.z != 0.0f) ? scale.z / parentScale.z : 0.0f;

        SetLocalScale(newLocalScale);
    }
    else
    {
        SetLocalScale(scale);
    }
}

Vec3f TransformComponent::GetWorldScale() const
{
    if (!p_gameObject)
        return m_localScale;
    auto parent = p_gameObject->GetParent();
    if (parent)
    {
        Vec3f parentScale = parent->GetTransform()->GetWorldScale();
        return parentScale * m_localScale;
    }
    return m_localScale;
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

void TransformComponent::UpdateMatrix()
{
    if (!m_dirty) 
        return;
    if (!p_gameObject)
    {
        ComputeModelMatrix();
        return;
    }
    if (SafePtr<GameObject> parent = p_gameObject->GetParent())
    {
        ComputeModelMatrix(parent->GetTransform()->GetWorldMatrix());
    }
    else
    {
        ComputeModelMatrix();
    }
    
    for (auto& child : p_gameObject->GetChildren())
    {
        child->GetTransform()->UpdateMatrix();
    }
}

void TransformComponent::ComputeModelMatrix(const Mat4& parentMatrix)
{
    UpdateModelMatrix(parentMatrix * GetLocalMatrix());
}

void TransformComponent::ComputeModelMatrix()
{
    UpdateModelMatrix(GetLocalMatrix());
}

void TransformComponent::UpdateModelMatrix(const Mat4& matrix)
{
    m_modelMatrix = matrix;
    EOnUpdateModelMatrix.Invoke();
    m_dirty = false;
}
