#include "IComponent.h"

#include "Debug/Log.h"

IComponent::~IComponent()
{
    PrintLog("Component %s destroyed", GetTypeName());
}
