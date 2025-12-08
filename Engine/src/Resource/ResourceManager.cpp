#include "ResourceManager.h"

#include "Material.h"
#include "Texture.h"
#include "Shader.h"
#include "Model.h"
#include "Mesh.h"

class Shader;

void ResourceManager::Initialize(RHIRenderer* renderer)
{
    m_renderer = renderer;
}

void ResourceManager::LoadDefaultShader(const std::filesystem::path& shaderPath)
{
    Load<Shader>(shaderPath, false);
    
    m_defaultShader = GetHash(shaderPath);
}

void ResourceManager::LoadDefaultTexture(const std::filesystem::path& texturePath)
{
    SafePtr<Texture> texture = Load<Texture>(texturePath, false);
    
    m_defaultTexture = GetHash(texturePath);
    
    m_renderer->SetDefaultTexture(texture);
}

void ResourceManager::LoadDefaultMaterial(const std::filesystem::path& materialPath)
{
    std::shared_ptr<Material> material = std::make_shared<Material>(materialPath);

    std::shared_ptr<Shader> shader = GetDefaultShader();
    material->SetShader(shader);
    
    material->SetAttribute("color", Vec4f(1.f, 1.f, 1.f, 1.f));
    
    m_defaultMaterial = GetHash(materialPath);
    
    AddResource(material);
}

std::shared_ptr<Shader> ResourceManager::GetDefaultShader() const
{
    return GetResource<Shader>(m_defaultShader);
}

std::shared_ptr<Texture> ResourceManager::GetDefaultTexture() const
{
    return GetResource<Texture>(m_defaultTexture);
}

std::shared_ptr<Material> ResourceManager::GetDefaultMaterial() const
{
    return GetResource<Material>(m_defaultMaterial);
}
