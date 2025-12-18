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
    
    if (!File::Exist(p_path))
    {
        PrintError("Failed to read shader source from file: %s", p_path.c_str());
        return false;
    }

    std::filesystem::path compiledPath = GetCompiledPath();
    if (File::Exist(compiledPath) && File::GetLastWriteTime(compiledPath) > File::GetLastWriteTime(p_path))
    {
        File file(compiledPath);
        if (!file.ReadAllText(p_content))
        {
            PrintError("Failed to read compiled shader from file: %s", compiledPath.c_str());
            return false;
        }
        p_compiled = true;
    }
    else
    {
        File file(p_path);
        if (!file.ReadAllText(p_content))
        {
            PrintError("Failed to read shader source from file: %s", p_path.c_str());
            return false;
        }
    }
    return true;
}

bool BaseShader::SendToGPU(RHIRenderer* renderer)
{
    if (!p_buffer)
    {
        if (!p_compiled)
        {
            PrintLog("Compiling shader: %s", p_path.generic_string().c_str());
            p_content = renderer->CompileShader(GetShaderType(), p_content);
            if (p_content.empty())
            {
                PrintError("Failed to compile shader: %s", p_path.c_str());
                return false;
            }
        
            File compiled(GetCompiledPath());
            if (!compiled.WriteAllText(p_content))
            {
                PrintError("Failed to write compiled shader to file: %s", GetCompiledPath().c_str());
                return false;
            }
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
std::filesystem::path BaseShader::GetCompiledPath() const
{
    std::filesystem::path cachePath = ResourceManager::GetCompiledCacheDir();

    return cachePath / (std::to_string(p_uuid) + ".compiled");
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
    m_vertexShader->EOnSentToGPU.Bind([this](){OnShaderSent();});
    
    m_fragmentShader = resourceManager->Load<FragmentShader>(fragmentShaderPath);
    if (!m_fragmentShader)
    {
        PrintError("Failed to load fragment shader: %s", fragmentShaderPath.c_str());
        return false;
    }
    m_fragmentShader->EOnSentToGPU.Bind([this](){OnShaderSent();});
    
    return true;
}

bool Shader::SendToGPU(RHIRenderer* renderer)
{
    if (!m_vertexShader->SentToGPU() || !m_fragmentShader->SentToGPU())
    {
        return false;
    }
    
    m_pushConstants = renderer->GetPushConstants(this);
    m_uniforms = renderer->GetUniforms(this);
    
    m_pipeline = renderer->CreatePipeline(this);
    return true;
}

void Shader::Unload()
{
}

void Shader::SendTexture(UBOBinding binding, Texture* texture, RHIRenderer* renderer)
{
    renderer->SendTexture(binding, texture, this);
}

void Shader::SendValue(UBOBinding binding, void* value, uint32_t size, RHIRenderer* renderer)
{
    renderer->SendValue(binding, value, size, this);
}

void Shader::OnShaderSent()
{
    if (m_vertexShader->SentToGPU() && m_fragmentShader->SentToGPU())
    {
        
    }
}
