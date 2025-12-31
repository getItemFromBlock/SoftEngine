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
    Int,
    Vec2f,
    Vec3f,
    Vec4f,
    Quat,
    Color3,
    Color4,
    Texture,
    Mesh,
    Material,
    Materials,
    Transform,
    ParticleSystem
};

struct Property
{
    std::string name; 
    PropertyType type;    
    void* data;
    
    std::function<void(void*)> setter = nullptr;
};

struct ClassDescriptor
{
    std::vector<Property> properties;
    
    Property& AddProperty(const char* name, PropertyType type, void* data);
    Property& AddFloat(const char* name, float& value);
    Property& AddInt(const char* name, int& value);
    Property& AddQuat(const char* name, Quat& value);
    Property& AddVec2f(const char* name, Vec2f& value);
    Property& AddVec3f(const char* name, Vec3f& value);
    Property& AddVec4f(const char* name, Vec4f& value);
    Property& AddTexture(const char* name, SafePtr<Texture>& value);
};