#include "Material.h"

#include <map>
#include <ranges>

#include "CubeMap.h"
#include "Shader.h"
#include "Core/Engine.h"
#include "Debug/Log.h"
#include "Render/Vulkan/VulkanMaterial.h"
#include "Render/Vulkan/VulkanRenderer.h"

bool Material::Load(ResourceManager* resourceManager)
{
    return true;
}

bool Material::SendToGPU(VulkanRenderer* renderer)
{
    return true;
}

void Material::Unload()
{
    if (m_handle)
    {
        m_handle->Cleanup();
        m_handle.reset();
    }
}

void Material::Describe(ClassDescriptor& descriptor)
{
    for (auto& [name, attrib] : m_attributes.floatAttributes)
        descriptor.AddFloat(name.c_str(), attrib.value);
    for (auto& [name, attrib] : m_attributes.intAttributes)
        descriptor.AddInt(name.c_str(), attrib.value);
    for (auto& [name, attrib] : m_attributes.vec2Attributes)
        descriptor.AddVec2f(name.c_str(), attrib.value);
    for (auto& [name, attrib] : m_attributes.vec3Attributes)
        descriptor.AddVec3f(name.c_str(), attrib.value);
    for (auto& [name, attrib] : m_attributes.vec4Attributes)
        descriptor.AddVec4f(name.c_str(), attrib.value);

    for (auto& [name, attrib] : m_attributes.samplerAttributes)
    {
        auto& prop = descriptor.AddTexture(name.c_str(), attrib.value);
        prop.setter = [this, prop](void* value)
        {
            auto* texture = static_cast<SafePtr<Texture>*>(value);
            SetAttribute(prop.name, *texture);
        };
    }
    for (auto& [name, attrib] : m_attributes.sampler3DAttributes)
    {
        auto& prop = descriptor.AddCubeMap(name.c_str(), attrib.value);
        prop.setter = [this, prop](void* value)
        {
            auto* cubeMap = static_cast<SafePtr<CubeMap>*>(value);
            SetAttribute(prop.name, *cubeMap);
        };
    }
}

void Material::SetShader(const SafePtr<Shader>& shader)
{
    if (m_shader && m_shader->GetUUID() != shader->GetUUID())
        m_shader->EOnSentToGPU.Unbind(m_shaderChangeEvent);

    m_shader = shader;
    m_shaderChangeEvent = m_shader->EOnSentToGPU.Bind([this]()
    {
        OnShaderChanged();
    });
}

void Material::SetAttribute(const std::string& name, float attribute)
{
    if (m_attributes.floatAttributes.contains(name))
        m_attributes.floatAttributes[name].value = attribute;
    else if (m_shader && !m_shader->SentToGPU())
        m_temporaryAttributes.floatAttributes[name] = attribute;
}

void Material::SetAttribute(const std::string& name, int attribute)
{
    if (m_attributes.intAttributes.contains(name))
        m_attributes.intAttributes[name].value = attribute;
    else if (m_shader && !m_shader->SentToGPU())
        m_temporaryAttributes.intAttributes[name] = attribute;
}

void Material::SetAttribute(const std::string& name, const Vec2f& attribute)
{
    if (m_attributes.vec2Attributes.contains(name))
        m_attributes.vec2Attributes[name].value = attribute;
    else if (m_shader && !m_shader->SentToGPU())
        m_temporaryAttributes.vec2Attributes[name] = attribute;
}

void Material::SetAttribute(const std::string& name, const Vec3f& attribute)
{
    if (m_attributes.vec3Attributes.contains(name))
        m_attributes.vec3Attributes[name].value = attribute;
    else if (m_shader && !m_shader->SentToGPU())
        m_temporaryAttributes.vec3Attributes[name] = attribute;
}

void Material::SetAttribute(const std::string& name, const Vec4f& attribute)
{
    if (m_attributes.vec4Attributes.contains(name))
        m_attributes.vec4Attributes[name].value = attribute;
    else if (m_shader && !m_shader->SentToGPU())
        m_temporaryAttributes.vec4Attributes[name] = attribute;
}

void Material::SetAttribute(const std::string& name, const Mat4& attribute)
{
    if (m_attributes.matrixAttributes.contains(name))
        m_attributes.matrixAttributes[name].value = attribute;
    else if (m_shader && !m_shader->SentToGPU())
        m_temporaryAttributes.matrixAttributes[name] = attribute;
}

void Material::SetAttribute(const std::string& name, const SafePtr<Texture>& texture)
{
    if (m_attributes.samplerAttributes.contains(name))
    {
        m_attributes.samplerAttributes[name].value = texture;
        if (!texture || !m_shader) return;

        m_shader->EOnSentToGPU.Bind([this, texture, name]()
        {
            texture->EOnSentToGPU.Bind([this, texture, name]()
            {
                if (m_shader) 
                    m_attributesToSync[name] = 0;
            });
        });
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.samplerAttributes[name] = texture;
    }
}

void Material::SetAttribute(const std::string& name, const SafePtr<CubeMap>& cubeMap)
{
    if (m_attributes.sampler3DAttributes.contains(name))
    {
        m_attributes.sampler3DAttributes[name].value = cubeMap;
        if (!cubeMap || !m_shader) 
            return;

        m_shader->EOnSentToGPU.Bind([this, cubeMap, name]()
        {
            cubeMap->EOnSentToGPU.Bind([this, cubeMap, name]()
            {
                if (m_shader) 
                    m_attributesToSync[name] = 0;
            });
        });
    }
    else if (m_shader && !m_shader->SentToGPU())
    {
        m_temporaryAttributes.sampler3DAttributes[name] = cubeMap;
    }
}

void Material::BakeWriteEntries()
{
    m_writeEntries.clear();
    m_uniformScratch.clear();

    Uniforms uniforms = m_shader->GetUniforms();

    for (auto& uniform : uniforms | std::views::values)
    {
        if (uniform.type != UniformType::NestedStruct &&
            uniform.type != UniformType::StorageBuffer)
            continue;

        auto key = std::make_pair(uniform.set, uniform.binding);
        auto& buf = m_uniformScratch[key];
        if (buf.size() < uniform.size)
            buf.assign(uniform.size, 0);
    }

    auto Bake = [&](const std::string& uniformName,
                    const std::string& memberName,
                    const void* srcPtr,
                    size_t size)
    {
        auto uniformIt = uniforms.find(uniformName);
        if (uniformIt == uniforms.end()) return;

        const Uniform& uniform = uniformIt->second;
        auto key = std::make_pair(uniform.set, uniform.binding);

        auto scratchIt = m_uniformScratch.find(key);
        if (scratchIt == m_uniformScratch.end()) return;

        uint32_t memberOffset = 0;
        for (const auto& member : uniform.members)
        {
            if (member.name == memberName)
            {
                memberOffset = member.offset;
                break;
            }
        }

        m_writeEntries.push_back({
            .src = srcPtr,
            .dst = scratchIt->second.data() + memberOffset,
            .size = size
        });
    };

    for (auto& [name, attrib] : m_attributes.floatAttributes)
        Bake(attrib.uniformName, name, &attrib.value, sizeof(float));

    for (auto& [name, attrib] : m_attributes.intAttributes)
        Bake(attrib.uniformName, name, &attrib.value, sizeof(int));

    for (auto& [name, attrib] : m_attributes.vec2Attributes)
        Bake(attrib.uniformName, name, &attrib.value, sizeof(Vec2f));

    for (auto& [name, attrib] : m_attributes.vec3Attributes)
        Bake(attrib.uniformName, name, &attrib.value, sizeof(Vec3f));

    for (auto& [name, attrib] : m_attributes.vec4Attributes)
        Bake(attrib.uniformName, name, &attrib.value, sizeof(Vec4f));

    for (auto& [name, attrib] : m_attributes.matrixAttributes)
        Bake(attrib.uniformName, name, &attrib.value, sizeof(Mat4));
}

void Material::SendAllValues(VulkanRenderer* renderer)
{
    if (!m_shader->IsLoaded() || !m_shader->SentToGPU())
        return;

    for (const auto& entry : m_writeEntries)
        std::memcpy(entry.dst, entry.src, entry.size);

    for (auto it = m_attributesToSync.begin(); it != m_attributesToSync.end();)
    {
        const auto& [attrib, frameProcessed] = *it;

        if (m_attributes.samplerAttributes.contains(attrib))
        {
            Uniform uniform = m_shader->GetUniform(attrib);
            SafePtr<Texture> texture = m_attributes.samplerAttributes.at(attrib).value;
            if (!texture)
                texture = Engine::Get()->GetResourceManager()->GetBlankTexture();

            m_handle->SetTextureForFrame(renderer->GetFrameIndex(),
                                         uniform.set, uniform.binding,
                                         texture.getPtr());

            it = (frameProcessed >= renderer->GetMaxFramesInFlight())
                     ? m_attributesToSync.erase(it)
                     : std::next(it);
        }
        else if (m_attributes.sampler3DAttributes.contains(attrib))
        {
            Uniform uniform = m_shader->GetUniform(attrib);
            SafePtr<CubeMap> cubeMap = m_attributes.sampler3DAttributes.at(attrib).value;
            
            if (!cubeMap)
                cubeMap = Engine::Get()->GetResourceManager()->GetBlankCubeMap();

            m_handle->SetCubemapForFrame(renderer->GetFrameIndex(),
                                         uniform.set, uniform.binding,
                                         cubeMap.getPtr());

            it = frameProcessed >= renderer->GetMaxFramesInFlight() ? m_attributesToSync.erase(it) : std::next(it);
        }
        else
        {
            ++it;
        }
    }

    for (auto& [key, buffer] : m_uniformScratch)
    {
        if (!buffer.empty())
        {
            m_handle->SetUniformData(key.first, key.second,
                                     buffer.data(), buffer.size(), renderer);
        }
    }
}

bool Material::Bind(VulkanRenderer* renderer)
{
    if (!m_handle) return false;
    m_handle->Bind(renderer);
    return true;
}

float Material::GetFloatAttribute(const std::string& name) const
{
    auto it = m_attributes.floatAttributes.find(name);
    return it != m_attributes.floatAttributes.end() ? it->second.value : 0.f;
}

int Material::GetIntAttribute(const std::string& name) const
{
    auto it = m_attributes.intAttributes.find(name);
    return it != m_attributes.intAttributes.end() ? it->second.value : 0;
}

Vec2f Material::GetVec2Attribute(const std::string& name) const
{
    auto it = m_attributes.vec2Attributes.find(name);
    return it != m_attributes.vec2Attributes.end() ? it->second.value : Vec2f::Zero();
}

Vec3f Material::GetVec3Attribute(const std::string& name) const
{
    auto it = m_attributes.vec3Attributes.find(name);
    return it != m_attributes.vec3Attributes.end() ? it->second.value : Vec3f::Zero();
}

Vec4f Material::GetVec4Attribute(const std::string& name) const
{
    auto it = m_attributes.vec4Attributes.find(name);
    return it != m_attributes.vec4Attributes.end() ? it->second.value : Vec4f::Zero();
}

SafePtr<Texture> Material::GetTexture(const std::string& name) const
{
    if (!m_handle) return {};
    return m_attributes.samplerAttributes.at(name).value;
}

void Material::OnShaderChanged()
{
    if (!m_shader || !m_shader->SentToGPU())
    {
        PrintWarning("Invalid shader, probably change");
        return;
    }

    m_attributes.Clear();
    m_writeEntries.clear();
    m_uniformScratch.clear();

    Uniforms uniforms = m_shader->GetUniforms();
    auto renderer = Engine::Get()->GetRenderer();

    if (m_handle)
    {
        m_handle->Cleanup();
        m_handle.reset();
    }
    m_handle = renderer->CreateMaterial(m_shader.getPtr());

    for (Uniform& uniform : uniforms | std::views::values)
    {
        switch (uniform.type)
        {
        case UniformType::StorageBuffer:
        case UniformType::NestedStruct:
            for (UniformMember& member : uniform.members)
            {
                switch (member.type)
                {
                case UniformType::Float:
                    {
                        float v = m_temporaryAttributes.floatAttributes.contains(member.name)
                                      ? m_temporaryAttributes.floatAttributes[member.name].value
                                      : 0.f;
                        m_attributes.floatAttributes[member.name] = Attribute<float>(uniform.name, v);
                    }
                    break;
                case UniformType::Int:
                    {
                        int v = m_temporaryAttributes.intAttributes.contains(member.name)
                                    ? m_temporaryAttributes.intAttributes[member.name].value
                                    : 0;
                        m_attributes.intAttributes[member.name] = Attribute<int>(uniform.name, v);
                    }
                    break;
                case UniformType::Vec2:
                    {
                        Vec2f v = m_temporaryAttributes.vec2Attributes.contains(member.name)
                                      ? m_temporaryAttributes.vec2Attributes[member.name].value
                                      : Vec2f::Zero();
                        m_attributes.vec2Attributes[member.name] = Attribute<Vec2f>(uniform.name, v);
                    }
                    break;
                case UniformType::Vec3:
                    {
                        Vec3f v = m_temporaryAttributes.vec3Attributes.contains(member.name)
                                      ? m_temporaryAttributes.vec3Attributes[member.name].value
                                      : Vec3f::Zero();
                        m_attributes.vec3Attributes[member.name] = Attribute<Vec3f>(uniform.name, v);
                    }
                    break;
                case UniformType::Vec4:
                    {
                        Vec4f v = m_temporaryAttributes.vec4Attributes.contains(member.name)
                                      ? m_temporaryAttributes.vec4Attributes[member.name].value
                                      : Vec4f::Zero();
                        m_attributes.vec4Attributes[member.name] = Attribute<Vec4f>(uniform.name, v);
                    }
                    break;
                case UniformType::Mat4:
                    {
                        Mat4 v = m_temporaryAttributes.matrixAttributes.contains(member.name)
                                     ? m_temporaryAttributes.matrixAttributes[member.name].value
                                     : Mat4::Identity();
                        m_attributes.matrixAttributes[member.name] = Attribute<Mat4>(uniform.name, v);
                    }
                    break;
                default: break;
                }
            }
            break;

        case UniformType::Sampler2D:
            {
                auto blank = Engine::Get()->GetResourceManager()->GetBlankTexture();
                m_attributes.samplerAttributes[uniform.name] =
                    m_temporaryAttributes.samplerAttributes.contains(uniform.name)
                        ? m_temporaryAttributes.samplerAttributes[uniform.name].value
                        : blank;

                SafePtr<Texture> texture = m_attributes.samplerAttributes[uniform.name].value;
                if (!texture) continue;
                texture->EOnSentToGPU.Bind([this, texture, uniform]()
                {
                    SendTexture(texture.getPtr(), uniform);
                });
            }
            break;

        case UniformType::SamplerCube:
            {
                auto blank = Engine::Get()->GetResourceManager()->GetBlankCubeMap();
                m_attributes.sampler3DAttributes[uniform.name] =
                    m_temporaryAttributes.sampler3DAttributes.contains(uniform.name)
                        ? m_temporaryAttributes.sampler3DAttributes[uniform.name].value
                        : blank;

                SafePtr<CubeMap> cubeMap = m_attributes.sampler3DAttributes[uniform.name].value;
                if (!cubeMap) continue;
                cubeMap->EOnSentToGPU.Bind([this, cubeMap, uniform]()
                {
                    SendCubeMap(cubeMap.getPtr(), uniform);
                });
            }
            break;

        default:
            PrintError("Unknown uniform type: %d", static_cast<int>(uniform.type));
            break;
        }
    }

    BakeWriteEntries();
}

void Material::SendTexture(Texture* texture, const Uniform& uniform) const
{
    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    m_handle->SetTexture(uniform.set, uniform.binding, texture, renderer);
    PrintLog("Send Texture %s to material %s",
             texture->GetPath().filename().generic_string().c_str(),
             GetPath().generic_string().c_str());
}

void Material::SendCubeMap(CubeMap* cubeMap, const Uniform& uniform) const
{
    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    m_handle->SetCubemap(uniform.set, uniform.binding, cubeMap, renderer);
    PrintLog("Send CubeMap %s to material %s",
             cubeMap->GetPath().filename().generic_string().c_str(),
             GetPath().generic_string().c_str());
}
