#include "GameObject.h"
SafePtr<GameObject> GameObject::GetParent()
{
    return m_scene.GetGameObject(m_parentUUID);
}
