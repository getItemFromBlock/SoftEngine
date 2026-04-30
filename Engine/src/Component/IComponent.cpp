#include "IComponent.h"

#include <nlohmann/json.hpp>

#include "Debug/Log.h"

IComponent::~IComponent()
{
    // PrintLog("Component %s destroyed", GetTypeName());
}

nlohmann::json IComponent::Serialize() const
{
    return {};
}
