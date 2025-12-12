#include "Material.h"

#include <ranges>

#include "Shader.h"
#include "Core/Engine.h"
#include "Debug/Log.h"
#include "Render/Vulkan/VulkanMaterial.h"
#include "Render/Vulkan/VulkanPipeline.h"
#include "Render/Vulkan/VulkanRenderer.h"

bool Material::Load(ResourceManager* resourceManager)
{
    return true;
}

bool Material::SendToGPU(RHIRenderer* renderer)
{
    return true;
}

void Material::Unload()
{
    m_handle->Cleanup();
}

void Material::SetShader(const SafePtr<Shader>& shader)
{
    m_shader = shader;

    m_shader->OnSentToGPU.Bind([this] { OnShaderChanged(); });
}

void Material::SetAttribute(const std::string& name, float attribute)
{
    if (m_attributes.floatAttributes.contains(name))
    {
        m_attributes.floatAttributes[name] = attribute;
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.floatAttributes[name] = attribute;
    }
}

void Material::SetAttribute(const std::string& name, int attribute)
{
    if (m_attributes.intAttributes.contains(name))
    {
        m_attributes.intAttributes[name] = attribute;
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.intAttributes[name] = attribute;
    }
}

void Material::SetAttribute(const std::string& name, const Vec2f& attribute)
{
    if (m_attributes.vec2Attributes.contains(name))
    {
        m_attributes.vec2Attributes[name] = attribute;
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.vec2Attributes[name] = attribute;
    }
}

void Material::SetAttribute(const std::string& name, const Vec3f& attribute)
{
    if (m_attributes.vec3Attributes.contains(name))
    {
        m_attributes.vec3Attributes[name] = attribute;
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.vec3Attributes[name] = attribute;
    }
}

void Material::SetAttribute(const std::string& name, const Vec4f& attribute)
{
    if (m_attributes.vec4Attributes.contains(name))
    {
        m_attributes.vec4Attributes[name] = attribute;
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.vec4Attributes[name] = attribute;
    }
}

void Material::SetAttribute(const std::string& name, const SafePtr<Texture>& texture)
{
    if (m_attributes.samplerAttributes.contains(name))
    {
        m_attributes.samplerAttributes[name] = texture;
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.samplerAttributes[name] = texture;
    }
}
void Material::SetAttribute(const std::string& name, const Mat4& attribute)
{
    if (m_attributes.matrixAttributes.contains(name))
    {
        m_attributes.matrixAttributes[name] = attribute;
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.matrixAttributes[name] = attribute;
    }
}

void Material::SendAllValues(RHIRenderer* renderer) const
{
    if (!m_shader->IsLoaded() || !m_shader->SentToGPU())
        return;
    struct UniformBuffer
    {
        std::vector<uint8_t> data;
    };

    std::unordered_map<std::string, UniformBuffer> customAttributes;

    auto AppendData = [&](const std::string& uniformName, const void* valuePtr, size_t size)
    {
        auto& buffer = customAttributes[uniformName].data;
        const uint8_t* bytePtr = static_cast<const uint8_t*>(valuePtr);
        buffer.insert(buffer.end(), bytePtr, bytePtr + size);
    };

    for (auto& attrib : m_attributes.floatAttributes)
    {
        AppendData(attrib.second.uniformName, &attrib.second.value, sizeof(float));
    }
    for (auto& attrib : m_attributes.intAttributes)
    {
        AppendData(attrib.second.uniformName, &attrib.second.value, sizeof(int));
    }
    for (auto& attrib : m_attributes.vec2Attributes)
    {
        AppendData(attrib.second.uniformName, &attrib.second.value, sizeof(Vec2f));
    }
    for (auto& attrib : m_attributes.vec3Attributes)
    {
        AppendData(attrib.second.uniformName, &attrib.second.value, sizeof(Vec3f));
    }
    for (auto& attrib : m_attributes.vec4Attributes)
    {
        AppendData(attrib.second.uniformName, &attrib.second.value, sizeof(Vec4f));
    }
    for (auto& attrib : m_attributes.matrixAttributes)
    {
        AppendData(attrib.second.uniformName, &attrib.second.value, sizeof(Mat4));
    }

    for (auto& uniformData : customAttributes)
    {
        Uniform uniform = m_shader->GetUniform(uniformData.first);
        auto binding = UBOBinding(uniform.set, uniform.binding);

        m_handle->SetUniformData(
            binding.set,
            binding.binding,
            uniformData.second.data.data(),
            uniformData.second.data.size(),
            renderer);
    }
}
void Material::Bind(RHIRenderer* renderer) const
{
    m_handle->Bind(renderer);
}
void Material::OnShaderChanged()
{
    m_attributes.Clear();
    Uniforms uniforms = m_shader->GetUniforms();

    for (Uniform& uniform : uniforms | std::views::values)
    {
        switch (uniform.type)
        {
            case UniformType::NestedStruct:
                for (UniformMember& member : uniform.members)
                {
                    switch (member.type)
                    {
                        case UniformType::Float:
                        {
                            float value = m_temporaryAttributes.floatAttributes.contains(member.name) ? m_temporaryAttributes.floatAttributes[member.name].value : 0.f;
                            m_attributes.floatAttributes[member.name] = Attribute<float>(
                                uniform.name,
                                value
                            );
                        }
                        break;
                        case UniformType::Int:
                        {
                            int value = m_temporaryAttributes.intAttributes.contains(member.name) ? m_temporaryAttributes.intAttributes[member.name].value : 0.f;
                            m_attributes.intAttributes[member.name] = Attribute<int>(
                                uniform.name,
                                value
                            );
                        }
                        break;
                        case UniformType::Vec2:
                        {
                            Vec2f value = m_temporaryAttributes.vec2Attributes.contains(member.name) ? m_temporaryAttributes.vec2Attributes[member.name].value : Vec2f::Zero();
                            m_attributes.vec2Attributes[member.name] = Attribute<Vec2f>(
                                uniform.name,
                                value
                            );
                        }
                        break;
                        case UniformType::Vec3:
                        {
                            Vec3f value = m_temporaryAttributes.vec3Attributes.contains(member.name) ? m_temporaryAttributes.vec3Attributes[member.name].value : Vec3f::Zero();
                            m_attributes.vec3Attributes[member.name] = Attribute<Vec3f>(
                                uniform.name,
                                value
                            );
                        }
                        break;
                        case UniformType::Vec4:
                        {
                            Vec4f value = m_temporaryAttributes.vec4Attributes.contains(member.name) ? m_temporaryAttributes.vec4Attributes[member.name].value : Vec4f::Zero();
                            m_attributes.vec4Attributes[member.name] = Attribute<Vec4f>(
                                uniform.name,
                                value
                            );
                        }
                        break;
                        case UniformType::Mat4:
                        {
                            Mat4 value = m_temporaryAttributes.matrixAttributes.contains(member.name) ? m_temporaryAttributes.matrixAttributes[member.name].value : Mat4::Identity();
                            m_attributes.matrixAttributes[member.name] = Attribute<Mat4>(
                                uniform.name,
                                value
                            );
                        }
                        break;
                    }
                }
                break;
            case UniformType::Sampler2D:
                m_attributes.samplerAttributes[uniform.name] =
                m_temporaryAttributes.samplerAttributes.contains(uniform.name) ? m_temporaryAttributes.samplerAttributes[uniform.name].value : SafePtr<Texture>{};
                break;
            case UniformType::SamplerCube:
                break;
            default:
                PrintError("Unknown uniform type: %d", static_cast<int>(uniform.type));
                break;
        }
    }
    auto renderer = Engine::Get()->GetRenderer();

    if (m_handle)
    {
        m_handle->Cleanup();
        m_handle.release();
    }
    m_handle = renderer->CreateMaterial(m_shader.get().get());
}
