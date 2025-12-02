#include "Shader.h"

#include "FragmentShader.h"
#include "ResourceManager.h"
#include "VertexShader.h"
#include "Debug/Log.h"
#include "Utils/File.h"

BaseShader::~BaseShader()
{
    if (p_buffer)
    {
        p_buffer->CleanUp();
    }
}

bool BaseShader::Load(ResourceManager* resourceManager)
{
    UNUSED(resourceManager);
    File file(p_path);
    if (!file.ReadAllText(p_content))
    {
        PrintError("Failed to read shader source from file: %s", p_path.c_str());
        return false;
    }
    return true;
}

bool BaseShader::SendToGPU(RHIRenderer* renderer)
{
    if (!p_buffer)
    {
        p_content = renderer->CompileShader(GetShaderType(), p_content);
        if (p_content.empty())
        {
            PrintError("Failed to compile shader: %s", p_path.c_str());
            return false;
        }
        p_buffer = renderer->CreateShaderBuffer(p_content);
        if (!p_buffer)
        {
            PrintError("Failed to create shader buffer: %s", p_path.c_str());
            return false;
        }
    }
    return true;
}

bool Shader::Load(ResourceManager* resourceManager)
{
    File file(p_path);
    std::vector<std::string> shaders;
    if (!file.ReadAllLines(shaders))
    {
        PrintError("Failed to read shader source from file: %s", p_path.c_str());
        return false;
    }

    std::string vertexShaderPath = shaders[0];
    std::string fragmentShaderPath = shaders[1];
    if (!File::Exist(vertexShaderPath))
    {
        vertexShaderPath = (p_path.parent_path() / vertexShaderPath).generic_string();
    }
    if (!File::Exist(fragmentShaderPath))
    {
        fragmentShaderPath = (p_path.parent_path() / fragmentShaderPath).generic_string();
    }
    
    m_vertexShader = resourceManager->Load<VertexShader>(vertexShaderPath);
    if (!m_vertexShader)
    {
        PrintError("Failed to load vertex shader: %s", vertexShaderPath.c_str());
        return false;
    }
    m_vertexShader->OnSentToGPU.Bind([this](){OnShaderSent();});
    
    m_fragmentShader = resourceManager->Load<FragmentShader>(fragmentShaderPath);
    if (!m_fragmentShader)
    {
        PrintError("Failed to load fragment shader: %s", fragmentShaderPath.c_str());
        return false;
    }
    m_fragmentShader->OnSentToGPU.Bind([this](){OnShaderSent();});
    
    return true;
}

bool Shader::SendToGPU(RHIRenderer* renderer)
{
    if (!m_vertexShader->SentToGPU() || !m_fragmentShader->SentToGPU())
    {
        return false;
    }
    
    m_uniforms = renderer->GetUniforms(this);
    
    m_pipeline = renderer->CreatePipeline(m_vertexShader.get().get(), m_fragmentShader.get().get(), m_uniforms);
    
    return true;
}

void Shader::Unload()
{
}

void Shader::SendValue(void* value, uint32_t size, RHIRenderer* renderer)
{
    renderer->SendValue(value, size, this);
}

void Shader::OnShaderSent()
{
    if (m_vertexShader->SentToGPU() && m_fragmentShader->SentToGPU())
    {
        
    }
}
