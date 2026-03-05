#pragma once
#include <functional>
#include <string>
#include <vector>

#include <galaxymath/Maths.h>

#include "Utils/Type.h"

class PostProcessShader;
class Shader;
class Material;
class Mesh;
class CubeMap;
class Texture;
class IResource;

enum class PropertyType
{
    None,
    Button,
    Bool,
    Int,
    Vec2i,
    Vec3i,
    Vec4i,
    Float,
    Vec2f,
    Vec3f,
    Vec4f,
    Quat,
    Color3,
    Color4,
    Enum,
    Texture,
    CubeMap,
    Mesh,
    Material,
    Shader,
    PostProcessShader,
    ParticleSystem
};

inline size_t GetPropertyTypeSize(PropertyType type)
{
    switch (type)
    {
    case PropertyType::Bool: return sizeof(bool);
    case PropertyType::Int: return sizeof(int);
    case PropertyType::Vec2i: return sizeof(Vec2i);
    case PropertyType::Vec3i: return sizeof(Vec3i);
    case PropertyType::Vec4i: return sizeof(Vec4i);
    case PropertyType::Float: return sizeof(float);
    case PropertyType::Vec2f: return sizeof(Vec2f);
    case PropertyType::Vec3f: return sizeof(Vec3f);
    case PropertyType::Vec4f: return sizeof(Vec4f);
    case PropertyType::Quat: return sizeof(Quat);
    case PropertyType::Color3: return sizeof(Vec3f);
    case PropertyType::Color4: return sizeof(Vec4f);
    case PropertyType::Enum: return sizeof(int);
    case PropertyType::Texture: return sizeof(SafePtr<Texture>);
    case PropertyType::CubeMap: return sizeof(SafePtr<CubeMap>);
    case PropertyType::Mesh: return sizeof(SafePtr<Mesh>);
    case PropertyType::Material: return sizeof(SafePtr<Material>);
    default: return 0;
    }
}

inline const char* to_string(PropertyType e)
{
    switch (e)
    {
    case PropertyType::None: return "None";
    case PropertyType::Button: return "Button";
    case PropertyType::Bool: return "Bool";
    case PropertyType::Int: return "Int";
    case PropertyType::Vec2i: return "Vec2i";
    case PropertyType::Vec3i: return "Vec3i";
    case PropertyType::Vec4i: return "Vec4i";
    case PropertyType::Float: return "Float";
    case PropertyType::Vec2f: return "Vec2f";
    case PropertyType::Vec3f: return "Vec3f";
    case PropertyType::Vec4f: return "Vec4f";
    case PropertyType::Quat: return "Quat";
    case PropertyType::Color3: return "Color3";
    case PropertyType::Color4: return "Color4";
    case PropertyType::Enum: return "Enum";
    case PropertyType::Texture: return "Texture";
    case PropertyType::CubeMap: return "CubeMap";
    case PropertyType::Mesh: return "Mesh";
    case PropertyType::Material: return "Material";
    case PropertyType::ParticleSystem: return "ParticleSystem";
    default: return "unknown";
    }
}

struct Property
{
    std::string name;
    std::string description;
    PropertyType type;
    void* data;
    const char *dataDescriptor = nullptr;

    std::function<void(void*)> setter;

    bool isList = false;
    bool readOnly = false;
    bool hasRange = false;

    union
    {
        struct
        {
            int minInt, maxInt;
        } intRange;

        struct
        {
            float minFloat, maxFloat;
        } floatRange;
    } range;

    std::function<void()> onModified;

    float dragSpeed = 0.01f;
    const char* format = "%.3f";

    Property() : type(PropertyType::None), data(nullptr)
    {
    }

    Property &SetRangeFloat(float minF, float maxF)
    {
        range.floatRange.minFloat = minF;
        range.floatRange.maxFloat = maxF;
        hasRange = true;
        return *this;
    }

    Property &SetRangeInt(int minI, int maxI)
    {
        range.intRange.minInt = minI;
        range.intRange.maxInt = maxI;
        hasRange = true;
        return *this;
    }
};

struct ClassDescriptor
{
    std::vector<Property> properties;

    Property& AddProperty(const char* name, PropertyType type, void* data);
    Property& AddProperty(const Property& property);
    Property& AddButton(const char* name);
    Property& AddBool(const char* name, bool& value);
    Property& AddFloat(const char* name, float& value);
    Property& AddVec2f(const char* name, Vec2f& value);
    Property& AddVec3f(const char* name, Vec3f& value);
    Property& AddVec4f(const char* name, Vec4f& value);
    Property& AddColor3(const char* name, Vec3f& value);
    Property& AddColor4(const char* name, Vec4f& value);
    Property& AddInt(const char* name, int& value);
    Property& AddVec2i(const char* name, Vec2i& value);
    Property& AddVec3i(const char* name, Vec3i& value);
    Property& AddVec4i(const char* name, Vec4i& value);
    Property& AddQuat(const char* name, Quat& value);
    Property& AddEnum(const char* name, int32_t* value, const char* description);
    Property& AddTexture(const char* name, SafePtr<Texture>& value);
    Property& AddCubeMap(const char* name, SafePtr<CubeMap>& value);
    Property& AddMesh(const char* name, SafePtr<Mesh>& value);
    Property& AddShader(const char* name, SafePtr<Shader>& value);
    Property& AddPostProcessShader(const char* name, SafePtr<PostProcessShader>& value);
};

class IDescribe
{
public:
    virtual ~IDescribe() = default;
    virtual void Describe(ClassDescriptor& descriptor) = 0;
};
