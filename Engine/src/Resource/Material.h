#pragma once
#include <memory>
#include <galaxymath/Maths.h>

#include "IResource.h"

#include "Utils/Type.h"
#include "Render/Vulkan/VulkanMaterial.h"

#include "Render/RHI/RHIMaterial.h"

class Shader;
class Texture;

struct CustomAttributes
{
    void* data = nullptr;
    size_t size = 0;
};

template<typename T>
struct Attribute
{
    std::string uniformName;
    T value;
    
    Attribute() = default;
    Attribute(const std::string& uniformName, T value) : uniformName(uniformName), value(value) {}
    
    void operator=(const T& value) { this->value = value; }
    void operator=(const Attribute& attribute) { this->uniformName = attribute.uniformName, this->value = attribute.value; }
};

struct MaterialAttributes
{
    std::unordered_map<std::string, Attribute<float>> floatAttributes;
    std::unordered_map<std::string, Attribute<int>> intAttributes;
    std::unordered_map<std::string, Attribute<Vec2f>> vec2Attributes;
    std::unordered_map<std::string, Attribute<Vec3f>> vec3Attributes;
    std::unordered_map<std::string, Attribute<Vec4f>> vec4Attributes;
    std::unordered_map<std::string, Attribute<SafePtr<Texture>>> samplerAttributes;
    std::unordered_map<std::string, Attribute<Mat4>> matrixAttributes;
    
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
    
    ResourceType GetResourceType() const override { return ResourceType::Material; }
    
    void SetShader(const SafePtr<Shader>& shader);
    SafePtr<Shader> GetShader() const { return m_shader; }

    void SetAttribute(const std::string& name, float attribute);
    void SetAttribute(const std::string& name, int attribute);
    void SetAttribute(const std::string& name, const Vec2f& attribute);
    void SetAttribute(const std::string& name, const Vec3f& attribute);
    void SetAttribute(const std::string& name, const Vec4f& attribute);
    void SetAttribute(const std::string& name, const SafePtr<Texture>& texture);
    void SetAttribute(const std::string& name, const Mat4& attribute);

    void SendAllValues(RHIRenderer* renderer) const;
    
    void Bind(RHIRenderer* renderer) const;

private:
    void OnShaderChanged();
private:
    std::unique_ptr<RHIMaterial> m_handle;
    SafePtr<Shader> m_shader;
    
    MaterialAttributes m_attributes;
    MaterialAttributes m_temporaryAttributes;
};
