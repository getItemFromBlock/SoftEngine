#pragma once
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <cstdint>

#include "Component/IComponent.h"

using ComponentID = uint64_t;
class ComponentRegister
{
public:
    template <typename T>
    void RegisterComponent()
    {
        static_assert(std::is_base_of_v<IComponent, T>, "T must inherit from IComponent");
        ComponentID id = GetComponentID<T>();

        m_components[id] = std::make_unique<T>();
    }

    template <typename T>
    static ComponentID GetComponentID()
    {
        static const ComponentID id = s_nextID++;
        return id;
    }

private:
    std::unordered_map<ComponentID, std::unique_ptr<IComponent>> m_components;
    inline static ComponentID s_nextID = 0;
};

