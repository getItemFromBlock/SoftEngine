#pragma once
#include <galaxymath/Maths.h>

#include "IResource.h"

#include "Utils/Type.h"

class Shader;
class Texture;

struct MaterialAttributes
{
    std::unordered_map<std::string, float> floatAttributes;
    std::unordered_map<std::string, int> intAttributes;
    std::unordered_map<std::string, Vec2f> vec2Attributes;
    std::unordered_map<std::string, Vec3f> vec3Attributes;
    std::unordered_map<std::string, Vec4f> vec4Attributes;
    std::unordered_map<std::string, SafePtr<Texture>> samplerAttributes;
    
    void Clear()
    {
        floatAttributes.clear();
        intAttributes.clear();
        vec2Attributes.clear();
        vec3Attributes.clear();
        vec4Attributes.clear();
        samplerAttributes.clear();
    }
};

class Material : public IResource
{
public:
    using IResource::IResource;
    
    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(RHIRenderer* renderer) override;
    void Unload() override;
    
    void SetShader(const SafePtr<Shader>& shader);
    SafePtr<Shader> GetShader() const { return m_shader; }

    void SetAttribute(const std::string& name, float attribute);
    void SetAttribute(const std::string& name, int attribute);
    void SetAttribute(const std::string& name, const Vec2f& attribute);
    void SetAttribute(const std::string& name, const Vec3f& attribute);
    void SetAttribute(const std::string& name, const Vec4f& attribute);
    void SetAttribute(const std::string& name, const SafePtr<Texture>& texture);
private:
    SafePtr<Shader> m_shader;
    
    MaterialAttributes m_attributes;
    MaterialAttributes m_temporaryAttributes;
};
