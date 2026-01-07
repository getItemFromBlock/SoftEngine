#include "Material.h"

#include <map>
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
    if (m_handle)
        m_handle->Cleanup();
}

std::string Material::GetName(bool extension) const
{
    return IResource::GetName(extension);
}

void Material::Describe(ClassDescriptor& descriptor)
{
    for (auto& [name, attrib] : m_attributes.floatAttributes)
    {
        descriptor.AddFloat(name.c_str(), attrib.value);
    }
    for (auto& [name, attrib] : m_attributes.intAttributes)
    {
        descriptor.AddInt(name.c_str(), attrib.value);
    }
    for (auto& [name, attrib] : m_attributes.vec2Attributes)
    {
        descriptor.AddVec2f(name.c_str(), attrib.value);
    }
    for (auto& [name, attrib] : m_attributes.vec3Attributes)
    {
        descriptor.AddVec3f(name.c_str(), attrib.value);
    }
    for (auto& [name, attrib] : m_attributes.vec4Attributes)
    {
        descriptor.AddVec4f(name.c_str(), attrib.value);
    }
    for (auto& [name, attrib] : m_attributes.samplerAttributes)
    {
        auto& prop = descriptor.AddTexture(name.c_str(), attrib.value);
        prop.setter = [this, prop](void* value)
        {
            SafePtr<Texture>* texture = static_cast<SafePtr<Texture>*>(value);
            SetAttribute(prop.name, *texture);
        };
    }
}

void Material::SetShader(const SafePtr<Shader>& shader)
{
    if (m_shader && m_shader->GetUUID() != shader->GetUUID())
    {
        m_shader->EOnSentToGPU.Unbind(m_shaderChangeEvent);
    }

    m_shader = shader;
    m_shaderChangeEvent = m_shader->EOnSentToGPU.Bind([this]()
    {
        OnShaderChanged();
    });
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
        m_dirty = true;
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

    std::map<std::pair<uint32_t, uint32_t>, UniformBuffer> uniformBuffers;

    auto AppendData = [&](const std::string& uniformName, const std::string& memberName, const void* valuePtr, size_t size)
    {
        Uniform uniform = m_shader->GetUniform(uniformName);
        auto key = std::make_pair(uniform.set, uniform.binding);
        auto& buffer = uniformBuffers[key];

        // Find the member with matching name
        uint32_t memberOffset = 0;
        bool found = false;
        for (const auto& member : uniform.members)
        {
            if (member.name == memberName)
            {
                memberOffset = member.offset;
                found = true;
                break;
            }
        }

        if (!found)
        {
            // Member not found, skip this attribute
            return;
        }

        // Ensure buffer is large enough to hold data at memberOffset + size
        size_t requiredSize = memberOffset + size;
        if (buffer.data.size() < requiredSize)
        {
            buffer.data.resize(requiredSize, 0);
        }

        // Copy data at the correct offset
        const uint8_t* bytePtr = static_cast<const uint8_t*>(valuePtr);
        std::memcpy(buffer.data.data() + memberOffset, bytePtr, size);
    };

    for (auto& attrib : m_attributes.floatAttributes)
    {
        AppendData(attrib.second.uniformName, attrib.first, &attrib.second.value, sizeof(float));
    }
    for (auto& attrib : m_attributes.intAttributes)
    {
        AppendData(attrib.second.uniformName, attrib.first, &attrib.second.value, sizeof(int));
    }
    for (auto& attrib : m_attributes.vec2Attributes)
    {
        AppendData(attrib.second.uniformName, attrib.first, &attrib.second.value, sizeof(Vec2f));
    }
    for (auto& attrib : m_attributes.vec3Attributes)
    {
        AppendData(attrib.second.uniformName, attrib.first, &attrib.second.value, sizeof(Vec3f));
    }
    for (auto& attrib : m_attributes.vec4Attributes)
    {
        AppendData(attrib.second.uniformName, attrib.first, &attrib.second.value, sizeof(Vec4f));
    }
    for (auto& attrib : m_attributes.matrixAttributes)
    {
        AppendData(attrib.second.uniformName, attrib.first, &attrib.second.value, sizeof(Mat4));
    }

    for (auto& [binding, buffer] : uniformBuffers)
    {
        m_handle->SetUniformData(
            binding.first,
            binding.second,
            buffer.data.data(),
            buffer.data.size(),
            renderer);
    }
}

bool Material::Bind(RHIRenderer* renderer)
{
    if (!m_handle)
        return false;
    
    if (m_dirty)
    {
        for (auto& [name, buffer] : m_attributes.samplerAttributes)
        {
            auto texture = buffer.value;
            if (!texture)
                continue;
            texture->EOnSentToGPU.Bind([this, texture, name]()
            {
                auto uniform = m_shader->GetUniform(name);

                SendTexture(texture.getPtr(), uniform);
            });
        }
        auto frameCount = Cast<VulkanRenderer>(renderer)->GetMaxFramesInFlight();
        m_frameProcessed++;
        
        if (m_frameProcessed >= frameCount)
        {
            m_dirty = false;
            m_frameProcessed = 0;
        }
    }
    
    m_handle->Bind(renderer);
    return true;
}

void Material::OnShaderChanged()
{
    if (!m_shader || !m_shader->SentToGPU())
    {
        PrintWarning("Invalid shader, probably change");
        return;
    }
    m_attributes.Clear();
    Uniforms uniforms = m_shader->GetUniforms();

    auto renderer = Engine::Get()->GetRenderer();

    if (m_handle)
    {
        m_handle->Cleanup();
        m_handle.release();
    }
    m_handle = renderer->CreateMaterial(m_shader.getPtr());

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
                        float value = m_temporaryAttributes.floatAttributes.contains(member.name)
                                          ? m_temporaryAttributes.floatAttributes[member.name].value
                                          : 0.f;
                        m_attributes.floatAttributes[member.name] = Attribute<float>(
                            uniform.name,
                            value
                        );
                    }
                    break;
                case UniformType::Int:
                    {
                        int value = m_temporaryAttributes.intAttributes.contains(member.name)
                                        ? m_temporaryAttributes.intAttributes[member.name].value
                                        : 0.f;
                        m_attributes.intAttributes[member.name] = Attribute<int>(
                            uniform.name,
                            value
                        );
                    }
                    break;
                case UniformType::Vec2:
                    {
                        Vec2f value = m_temporaryAttributes.vec2Attributes.contains(member.name)
                                          ? m_temporaryAttributes.vec2Attributes[member.name].value
                                          : Vec2f::Zero();
                        m_attributes.vec2Attributes[member.name] = Attribute<Vec2f>(
                            uniform.name,
                            value
                        );
                    }
                    break;
                case UniformType::Vec3:
                    {
                        Vec3f value = m_temporaryAttributes.vec3Attributes.contains(member.name)
                                          ? m_temporaryAttributes.vec3Attributes[member.name].value
                                          : Vec3f::Zero();
                        m_attributes.vec3Attributes[member.name] = Attribute<Vec3f>(
                            uniform.name,
                            value
                        );
                    }
                    break;
                case UniformType::Vec4:
                    {
                        Vec4f value = m_temporaryAttributes.vec4Attributes.contains(member.name)
                                          ? m_temporaryAttributes.vec4Attributes[member.name].value
                                          : Vec4f::Zero();
                        m_attributes.vec4Attributes[member.name] = Attribute<Vec4f>(
                            uniform.name,
                            value
                        );
                    }
                    break;
                case UniformType::Mat4:
                    {
                        Mat4 value = m_temporaryAttributes.matrixAttributes.contains(member.name)
                                         ? m_temporaryAttributes.matrixAttributes[member.name].value
                                         : Mat4::Identity();
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
            {
                auto blankTexture = Engine::Get()->GetResourceManager()->GetBlankTexture();
                m_attributes.samplerAttributes[uniform.name] =
                    m_temporaryAttributes.samplerAttributes.contains(uniform.name)
                        ? m_temporaryAttributes.samplerAttributes[uniform.name].value
                        : blankTexture;

                m_dirty = true;
            }
            break;
        case UniformType::SamplerCube:
            break;
        default:
            PrintError("Unknown uniform type: %d", static_cast<int>(uniform.type));
            break;
        }
    }
}

void Material::SendTexture(Texture* texture, const Uniform& uniform) const
{
    RHIRenderer* renderer = Engine::Get()->GetRenderer();
    VulkanMaterial* rhiMat = dynamic_cast<VulkanMaterial*>(m_handle.get());
    auto currentFrame = Cast<VulkanRenderer>(renderer)->GetFrameIndex();
    rhiMat->SetTextureForFrame(currentFrame, uniform.set, uniform.binding, texture);
}
