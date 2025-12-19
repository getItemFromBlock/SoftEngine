#include "ClassDescriptor.h"

void ClassDescriptor::AddProperty(const char* name, PropertyType type, void* data)
{
    properties.push_back({ name, type, data });
}

void ClassDescriptor::AddFloat(const char* name, float& value)
{
    AddProperty(name, PropertyType::Float, &value);
}

void ClassDescriptor::AddInt(const char* name, int& value)
{
    AddProperty(name, PropertyType::Int, &value);
}

void ClassDescriptor::AddQuat(const char* name, Quat& value)
{
    AddProperty(name, PropertyType::Quat, &value);
}

void ClassDescriptor::AddVec2f(const char* name, Vec2f& value)
{
    AddProperty(name, PropertyType::Vec2f, &value);
}

void ClassDescriptor::AddVec3f(const char* name, Vec3f& value)
{
    AddProperty(name, PropertyType::Vec3f, &value);
}

void ClassDescriptor::AddVec4f(const char* name, Vec4f& value)
{
    AddProperty(name, PropertyType::Vec4f, &value);
}

void ClassDescriptor::AddTexture(const char* name, SafePtr<Texture>& value)
{
    AddProperty(name, PropertyType::Texture, &value);
}
