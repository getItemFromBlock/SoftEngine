#pragma once
#include <functional>
#include <string>
#include <vector>

#include <galaxymath/Maths.h>

#include "Utils/Type.h"

class Texture;
class IResource;

enum class PropertyType
{
    None,
    Bool,
    Float,
    Vec2f,
    Vec3f,
    Vec4f,
    Int,
    Vec2i,
    Vec3i,
    Vec4i,
    Quat,
    Color3,
    Color4,
    Enum,
    Texture,
    Mesh,
    Material,
    Materials,
    Transform,
    Button,
    ParticleSystem
};

struct Property
{
    std::string name; 
    PropertyType type = PropertyType::None;    
    void *data = nullptr;
    const void *desc = nullptr;
    
    std::function<void(void*)> setter = nullptr;
};

struct ClassDescriptor
{
    std::vector<Property> properties;
    
    Property& AddProperty(const char* name, PropertyType type, void* data);
    Property& AddFloat(const char* name, float& value);
    Property& AddVec2f(const char* name, Vec2f& value);
    Property& AddVec3f(const char* name, Vec3f& value);
    Property& AddVec4f(const char* name, Vec4f& value);
    Property& AddInt(const char* name, int& value);
    Property& AddVec2i(const char* name, Vec2i& value);
    Property& AddVec3i(const char* name, Vec3i& value);
    Property& AddVec4i(const char* name, Vec4i& value);
    Property& AddQuat(const char* name, Quat& value);
    Property& AddEnum(const char* name, int32_t* value, const char* description);
    Property& AddButton(const char* name, const std::function<void(void *)>& callback);
    Property& AddTexture(const char* name, SafePtr<Texture>& value);
};