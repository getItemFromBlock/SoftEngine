#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <type_traits>
#include <cstdint>
#include <optional>

#include "Component/IComponent.h"

struct ComponentTypeInfo
{
    std::unique_ptr<IComponent> (*Create)(GameObject*);
    const char* (*GetTypeName)();
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
            [](GameObject* gameObject) -> std::unique_ptr<IComponent> {
                return std::make_unique<T>(gameObject);
            },
            &T::GetStaticTypeName,
            [](IComponent* c, ClassDescriptor& d) {
                static_cast<T*>(c)->Describe(d);
            }
        };
        m_nameToId[T::GetStaticTypeName()] = id;
    }

    const ComponentTypeInfo* Get(ComponentID id) const
    {
        auto it = m_types.find(id);
        return it != m_types.end() ? &it->second : nullptr;
    }

    std::optional<ComponentID> GetComponentID(const std::string& typeName) const
    {
        auto it = m_nameToId.find(typeName);
        if (it == m_nameToId.end())
            return std::nullopt;
        return it->second;
    }

    std::shared_ptr<IComponent> CreateComponent(GameObject* gameObject, uint64_t id);

    template<typename T>
    static ComponentID GetComponentID()
    {
        static ComponentID id = s_nextID++;
        return id;
    }
    
    std::unordered_map<ComponentID, ComponentTypeInfo> GetComponentTypes() const { return m_types; }

private:
    std::unordered_map<ComponentID, ComponentTypeInfo> m_types;
    std::unordered_map<std::string, ComponentID> m_nameToId;
    inline static ComponentID s_nextID = 0;
};

