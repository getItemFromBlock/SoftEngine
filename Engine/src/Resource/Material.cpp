#include "Material.h"

#include "Shader.h"

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
}

void Material::SetShader(const SafePtr<Shader>& shader)
{
    m_shader = shader;

    auto lambda = [this]()
    {
        m_attributes.Clear();
        auto uniforms = m_shader->GetUniforms();
        
        for (Uniform& uniform : uniforms)
        {
            switch (uniform.type) {
            case UniformType::NestedStruct:
                for (UniformMember& member : uniform.members)
                {
                    switch (member.type)
                    {
                    case UniformType::Float:
                        m_attributes.floatAttributes[member.name] = 
                            m_temporaryAttributes.floatAttributes.contains(member.name) ? 
                            m_temporaryAttributes.floatAttributes[member.name] : 0.f;
                        break;
                    case UniformType::Int:
                        m_attributes.intAttributes[member.name] = 
                            m_temporaryAttributes.intAttributes.contains(member.name) ? 
                            m_temporaryAttributes.intAttributes[member.name] : 0;
                        break;
                    case UniformType::Vec2:
                        m_attributes.vec2Attributes[member.name] = 
                            m_temporaryAttributes.vec2Attributes.contains(member.name) ? 
                            m_temporaryAttributes.vec2Attributes[member.name] : Vec2f::Zero();
                        break;
                    case UniformType::Vec3:
                        m_attributes.vec3Attributes[member.name] = 
                            m_temporaryAttributes.vec3Attributes.contains(member.name) ? 
                            m_temporaryAttributes.vec3Attributes[member.name] : Vec3f::Zero();
                        break;
                    case UniformType::Vec4:
                        m_attributes.vec4Attributes[member.name] = 
                            m_temporaryAttributes.vec4Attributes.contains(member.name) ? 
                            m_temporaryAttributes.vec4Attributes[member.name] : Vec4f::Zero();
                        break;
                    }
                }
                break;
            case UniformType::Sampler2D:
                m_attributes.samplerAttributes[uniform.name] = 
                    m_temporaryAttributes.samplerAttributes.contains(uniform.name) ? 
                    m_temporaryAttributes.samplerAttributes[uniform.name] : SafePtr<Texture>{};
                break;
            case UniformType::SamplerCube:
                break;
            }
        }
    };
    if (m_shader->SentToGPU())
    {
        lambda();
    }
    else
    {
        m_shader->OnSentToGPU.Bind(lambda);
    }
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
