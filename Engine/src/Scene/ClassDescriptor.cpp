#include "ClassDescriptor.h"

Property& ClassDescriptor::AddProperty(const char* name, PropertyType type, void* data)
{
    Property property;
    property.name = name;
    property.type = type;
    property.data = data;
    properties.push_back(property);
    return properties.back();
}

Property& ClassDescriptor::AddProperty(const Property& property)
{
    properties.push_back(property);
    return properties.back();
}

Property& ClassDescriptor::AddBool(const char* name, bool& value)
{
    return AddProperty(name, PropertyType::Bool, &value);
}

Property& ClassDescriptor::AddFloat(const char* name, float& value)
{
    return AddProperty(name, PropertyType::Float, &value);
}

Property& ClassDescriptor::AddInt(const char* name, int& value)
{
    return AddProperty(name, PropertyType::Int, &value);
}

Property& ClassDescriptor::AddQuat(const char* name, Quat& value)
{
    return AddProperty(name, PropertyType::Quat, &value);
}

Property &ClassDescriptor::AddEnum(const char *name, int32_t *value, const char *description)
{
    Property &res = AddProperty(name, PropertyType::Enum, value);
    res.dataDescriptor = description;
    return res;
}

Property &ClassDescriptor::AddButton(const char *name)
{
    return AddProperty(name, PropertyType::Button, nullptr);
}

Property& ClassDescriptor::AddVec2f(const char* name, Vec2f& value)
{
    return AddProperty(name, PropertyType::Vec2f, &value);
}

Property& ClassDescriptor::AddVec3f(const char* name, Vec3f& value)
{
    return AddProperty(name, PropertyType::Vec3f, &value);
}

Property& ClassDescriptor::AddVec4f(const char* name, Vec4f& value)
{
    return AddProperty(name, PropertyType::Vec4f, &value);
}

Property &ClassDescriptor::AddVec2i(const char *name, Vec2i &value)
{
    return AddProperty(name, PropertyType::Vec2i, &value);
}

Property &ClassDescriptor::AddVec3i(const char *name, Vec3i &value)
{
    return AddProperty(name, PropertyType::Vec3i, &value);
}

Property &ClassDescriptor::AddVec4i(const char *name, Vec4i &value)
{
    return AddProperty(name, PropertyType::Vec4i, &value);
}

Property& ClassDescriptor::AddColor3(const char* name, Vec3f& value)
{
    return AddProperty(name, PropertyType::Color3, &value);
}

Property& ClassDescriptor::AddColor4(const char* name, Vec4f& value)
{
    return AddProperty(name, PropertyType::Color4, &value);
}

Property& ClassDescriptor::AddTexture(const char* name, SafePtr<Texture>& value)
{
    return AddProperty(name, PropertyType::Texture, &value);
}

Property& ClassDescriptor::AddCubeMap(const char* name, SafePtr<CubeMap>& value)
{
    return AddProperty(name, PropertyType::CubeMap, &value);
}

Property& ClassDescriptor::AddMesh(const char* name, SafePtr<Mesh>& value)
{
    return AddProperty(name, PropertyType::Mesh, &value);
}

Property& ClassDescriptor::AddShader(const char* name, SafePtr<Shader>& value)
{
    return AddProperty(name, PropertyType::Shader, &value);
}

Property& ClassDescriptor::AddPostProcessShader(const char* name, SafePtr<PostProcessShader>& value)
{
    return AddProperty(name, PropertyType::PostProcessShader, &value);
}
