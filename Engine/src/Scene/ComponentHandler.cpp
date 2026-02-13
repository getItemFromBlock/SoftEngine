#include "ComponentHandler.h"

std::shared_ptr<IComponent>  ComponentRegister::CreateComponent(GameObject* gameObject, uint64_t id)
{
    auto it = m_types.find(id);
    if (it == m_types.end())
        return nullptr;
    
    return it->second.Create(gameObject);
}
