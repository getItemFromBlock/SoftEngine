#pragma once
#include <map>
#include <memory>
#include <set>
#include <galaxymath/Maths.h>

#include "CubeMap.h"
#include "IResource.h"

#include "Utils/Type.h"
#include "Render/Vulkan/VulkanMaterial.h"

class CubeMap;
class Shader;
class Texture;

struct CustomAttributes
{
    void* data = nullptr;
    size_t size = 0;
};

template <typename T>
struct Attribute
{
    std::string uniformName;
    int flag = 0; // flag for cubemap sending
    T value;

    Attribute() = default;

    Attribute(const std::string& uniformName, T value) : uniformName(uniformName), value(value)
    {
    }

    void operator=(const T& _value) { this->value = _value; }

    void operator=(const Attribute& attribute)
    {
        this->uniformName = attribute.uniformName, this->value = attribute.value;
    }
};

struct MaterialAttributes
{
    std::unordered_map<std::string, Attribute<float>> floatAttributes;
    std::unordered_map<std::string, Attribute<int>> intAttributes;
    std::unordered_map<std::string, Attribute<Vec2f>> vec2Attributes;
    std::unordered_map<std::string, Attribute<Vec3f>> vec3Attributes;
    std::unordered_map<std::string, Attribute<Vec4f>> vec4Attributes;
    std::unordered_map<std::string, Attribute<SafePtr<Texture>>> samplerAttributes;
    std::unordered_map<std::string, Attribute<SafePtr<CubeMap>>> sampler3DAttributes;
    std::unordered_map<std::string, Attribute<Mat4>> matrixAttributes;

    void Clear()
    {
        floatAttributes.clear();
        intAttributes.clear();
        vec2Attributes.clear();
        vec3Attributes.clear();
        vec4Attributes.clear();
        samplerAttributes.clear();
        sampler3DAttributes.clear();
        matrixAttributes.clear();
    }
};

struct UBOWriteEntry
{
    const void* src; 
    void* dst;
    size_t size;
};

class Material : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(Material)

    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(VulkanRenderer* renderer) override;
    void Unload() override;

    void Describe(ClassDescriptor& descriptor) override;

    void SetShader(const SafePtr<Shader>& shader);
    SafePtr<Shader> GetShader() const { return m_shader; }

    void SetAttribute(const std::string& name, float attribute, bool optional = false);
    void SetAttribute(const std::string& name, int attribute, bool optional = false);
    void SetAttribute(const std::string& name, const Vec2f& attribute, bool optional = false);
    void SetAttribute(const std::string& name, const Vec3f& attribute, bool optional = false);
    void SetAttribute(const std::string& name, const Vec4f& attribute, bool optional = false);
    void SetAttribute(const std::string& name, const SafePtr<Texture>& texture, bool optional = false);
    void SetAttribute(const std::string& name, const SafePtr<CubeMap>& cubeMap, bool optional = false, CubeMap::SampleMode samplerMode = CubeMap::SampleMode::Environment);
    void SetAttribute(const std::string& name, const Mat4& attribute, bool optional = false);

    void SendAllValues(VulkanRenderer* renderer);

    bool Bind(VulkanRenderer* renderer);

    MaterialAttributes GetAttributes() const { return m_attributes; }
    float GetFloatAttribute(const std::string& name) const;
    int GetIntAttribute(const std::string& name) const;
    Vec2f GetVec2Attribute(const std::string& name) const;
    Vec3f GetVec3Attribute(const std::string& name) const;
    Vec4f GetVec4Attribute(const std::string& name) const;

    VulkanMaterial* GetHandle() const { return m_handle.get(); }

    SafePtr<Texture> GetTexture(const std::string& name) const;
    void InitAttributes(const std::vector<UniformMember>& members, const std::string& uniformName, const std::string& rootName);
    void SendUBOValues(VulkanRenderer* renderer);
private:
    void OnShaderChanged();
    void SendTexture(Texture* texture, const Uniform& uniform) const;
    void SendCubeMap(CubeMap* cubeMap, const Uniform& uniform) const;

    void BakeWriteEntries();

private:
    std::unique_ptr<VulkanMaterial> m_handle;
    SafePtr<Shader> m_shader;

    MaterialAttributes m_attributes;
    MaterialAttributes m_temporaryAttributes;

    std::unordered_map<std::string, uint32_t> m_attributesToSync;

    EventHandle m_shaderChangeEvent;

    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint8_t>> m_uniformScratch;
    std::vector<UBOWriteEntry> m_writeEntries;
};
