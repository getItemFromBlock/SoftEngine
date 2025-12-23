#include "Shader.h"

#include "VertexShader.h"
#include "FragmentShader.h"
#include "ComputeShader.h"
#include "ResourceManager.h"
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
    std::vector<std::string> stagePaths;

    if (!file.ReadAllLines(stagePaths) || stagePaths.empty())
    {
        PrintError("Failed to read shader source or file is empty: %s", p_path.c_str());
        return false;
    }

    auto ResolvePath = [&](const std::string& subPath) -> std::string {
        if (subPath.empty()) return "";
        if (File::Exist(subPath)) return subPath;
        return (p_path.parent_path() / subPath).generic_string();
    };

    // Index mapping: 0 = Vert, 1 = Frag, 2 = Compute
    
    // 1. Vertex Shader
    if (stagePaths.size() > 0 && !stagePaths[0].empty())
    {
        std::string path = ResolvePath(stagePaths[0]);
        m_vertexShader = resourceManager->Load<VertexShader>(path);
        if (!m_vertexShader) {
            PrintError("Failed to load Vertex Shader: %s", path.c_str());
            return false;
        }
    }

    // 2. Fragment Shader
    if (stagePaths.size() > 1 && !stagePaths[1].empty())
    {
        std::string path = ResolvePath(stagePaths[1]);
        m_fragmentShader = resourceManager->Load<FragmentShader>(path);
        if (!m_fragmentShader) {
            PrintError("Failed to load Fragment Shader: %s", path.c_str());
            return false;
        }
    }

    // 3. Compute Shader
    if (stagePaths.size() > 2 && !stagePaths[2].empty())
    {
        std::string path = ResolvePath(stagePaths[2]);
        m_computeShader = resourceManager->Load<ComputeShader>(path);
        if (!m_computeShader) {
            PrintError("Failed to load Compute Shader: %s", path.c_str());
            return false;
        }
    }

    bool hasGraphics = (m_vertexShader && m_fragmentShader);
    bool hasCompute = (m_computeShader.valid());

    if (!hasGraphics && !hasCompute)
    {
        PrintError("Shader %s is invalid: Must have (Vert + Frag) or (Compute)", p_path.c_str());
        return false;
    }
    
    m_graphic = hasGraphics;

    return true;
}

bool Shader::SendToGPU(RHIRenderer* renderer)
{
    if (m_graphic && (!m_vertexShader->SentToGPU() || !m_fragmentShader->SentToGPU()) 
        || !m_graphic && (!m_computeShader->SentToGPU()))
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

std::unique_ptr<ComputeDispatch> Shader::CreateDispatch(RHIRenderer* renderer)
{
    return std::move(renderer->CreateDispatch(this));
}
