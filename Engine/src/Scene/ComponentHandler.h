#pragma once
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <cstdint>

#include "Component/IComponent.h"

struct ComponentTypeInfo
{
    std::unique_ptr<IComponent> (*Create)();
    void (*Describe)(IComponent*, ClassDescriptor&);
};

using ComponentID = uint64_t;
class ComponentRegister
{
public:
    template<typename T>
    void RegisterComponent()
    {
        static_assert(std::is_base_of_v<IComponent, T>);

        ComponentID id = GetComponentID<T>();

        m_types[id] = {
            []() -> std::unique_ptr<IComponent> {
                return std::make_unique<T>();
            },
            [](IComponent* c, ClassDescriptor& d) {
                static_cast<T*>(c)->Describe(d);
            }
        };
    }

    const ComponentTypeInfo* Get(ComponentID id) const
    {
        auto it = m_types.find(id);
        return it != m_types.end() ? &it->second : nullptr;
    }

    template<typename T>
    static ComponentID GetComponentID()
    {
        static ComponentID id = s_nextID++;
        return id;
    }

private:
    std::unordered_map<ComponentID, ComponentTypeInfo> m_types;
    inline static ComponentID s_nextID = 0;
};

