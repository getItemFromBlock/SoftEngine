#include "ResourceManager.h"

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
    Load<Texture>(texturePath, false);
    
    m_defaultTexture = GetHash(texturePath);
}

std::shared_ptr<Shader> ResourceManager::GetDefaultShader() const
{
    return GetResource<Shader>(m_defaultShader);
}

std::shared_ptr<Texture> ResourceManager::GetDefaultTexture() const
{
    return GetResource<Texture>(m_defaultTexture);
}
