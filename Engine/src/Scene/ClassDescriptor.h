#pragma once
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
    Transform
};

struct Property
{
    std::string name; 
    PropertyType type;    
    void* data;
};

struct ClassDescriptor
{
    std::vector<Property> properties;
    
    void AddProperty(const char* name, PropertyType type, void* data);
    void AddFloat(const char* name, float& value);
    void AddInt(const char* name, int& value);
    void AddQuat(const char* name, Quat& value);
    void AddVec2f(const char* name, Vec2f& value);
    void AddVec3f(const char* name, Vec3f& value);
    void AddVec4f(const char* name, Vec4f& value);
    void AddTexture(const char* name, SafePtr<Texture>& value);
};