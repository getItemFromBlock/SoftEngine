#include "GameObject.h"

std::vector<SafePtr<IComponent>> GameObject::GetComponents() const
{
    return m_scene.GetComponents(this);
}

SafePtr<GameObject> GameObject::GetParent() const
{
    return m_scene.GetGameObject(m_parentUUID);
}
