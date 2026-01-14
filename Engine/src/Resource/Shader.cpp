#include "Shader.h"

#include "ResourceManager.h"
#include "VertexShader.h"
#include "FragmentShader.h"
#include "ComputeShader.h"

#include "Debug/Log.h"

#include "Render/Vulkan/VulkanRenderer.h"

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

bool BaseShader::SendToGPU(VulkanRenderer* renderer)
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
    bool multithread = !ThreadPool::IsMainThread();
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
    
    for (const auto& path : stagePaths)
    {
        std::filesystem::path resolvedPath = ResolvePath(path);
        if (File::Exist(resolvedPath))
        {
            std::string extension = resolvedPath.extension().generic_string();
            if (extension == ".vert")
            {
                if (m_vertexShader)
                {
                    PrintError("Vertex Shader already loaded: %s", resolvedPath.c_str());
                    return false;
                }
                m_vertexShader = resourceManager->Load<VertexShader>(resolvedPath, multithread);
                if (!m_vertexShader) {
                    PrintError("Failed to load Vertex Shader: %s", resolvedPath.c_str());
                    return false;
                }
            }
            else if (extension == ".frag")
            {
                if (m_fragmentShader)
                {
                    PrintError("Fragment Shader already loaded: %s", resolvedPath.c_str());
                    return false;
                }
                m_fragmentShader = resourceManager->Load<FragmentShader>(resolvedPath, multithread);
                if (!m_fragmentShader) {
                    PrintError("Failed to load Fragment Shader: %s", resolvedPath.c_str());
                    return false;
                }
            }
            else if (extension == ".comp")
            {
                if (m_computeShader)
                {
                    PrintError("Compute Shader already loaded: %s", resolvedPath.c_str());
                    return false;
                }
                m_computeShader = resourceManager->Load<ComputeShader>(resolvedPath, multithread);
                if (!m_computeShader) {
                    PrintError("Failed to load Compute Shader: %s", resolvedPath.c_str());
                    return false;
                }
            }
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

bool Shader::SendToGPU(VulkanRenderer* renderer)
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

void Shader::SendTexture(UBOBinding binding, Texture* texture, VulkanRenderer* renderer)
{
    renderer->SendTexture(binding, texture, this);
}

void Shader::SendValue(UBOBinding binding, void* value, uint32_t size, VulkanRenderer* renderer)
{
    renderer->SendValue(binding, value, size, this);
}

std::unique_ptr<ComputeDispatch> Shader::CreateDispatch(VulkanRenderer* renderer)
{
    return std::move(renderer->CreateDispatch(this));
}
