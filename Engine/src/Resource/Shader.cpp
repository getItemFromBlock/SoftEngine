#include "Shader.h"

#include "ResourceManager.h"
#include "VertexShader.h"
#include "FragmentShader.h"
#include "ComputeShader.h"

#include "Debug/Log.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Utils/File.h"

#include <cpp_serializer/CppSerializer.h>

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
    
    if (!File::Exists(p_path))
    {
        PrintError("Failed to read shader source from file: %s", p_path.c_str());
        return false;
    }

    std::filesystem::path compiledPath = GetCompiledPath();
    if (File::Exists(compiledPath) && File::GetLastWriteTime(compiledPath) > File::GetLastWriteTime(p_path))
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

    return cachePath / (std::to_string(std::filesystem::hash_value(p_path)) + ".compiled");
}

bool Shader::Load(ResourceManager* resourceManager)
{
    bool multithread = !ThreadPool::IsMainThread();
    
    auto ResolvePath = [&](const std::string& subPath) -> std::string {
        if (subPath.empty()) return "";
        if (File::Exists(subPath)) return subPath;
        return (p_path.parent_path() / subPath).generic_string();
    };
    
    CppSer::Parser parser(p_path);
    if (!parser.IsFileOpen())
    {
        PrintError("Failed to open shader file: %s", p_path.generic_string().c_str());
        return false;
    }
    if (auto vertPath = parser["vert"].As<std::string>(); !vertPath.empty())
    {
        std::filesystem::path resolvedPath = ResolvePath(vertPath);
        
        if (m_vertexShader)
        {
            PrintError("Vertex Shader already loaded: %s", resolvedPath.c_str());
            return false;
        }
        
        if (File::Exists(resolvedPath))
        {
            m_vertexShader = resourceManager->Load<VertexShader>(resolvedPath, multithread);
            if (!m_vertexShader) {
                PrintError("Failed to load Vertex Shader: %s", resolvedPath.c_str());
                return false;
            }
        }
    }
    if (auto fragPath = parser["frag"].As<std::string>(); !fragPath.empty())
    {
        std::filesystem::path resolvedPath = ResolvePath(fragPath);
        
        if (m_fragmentShader)
        {
            PrintError("Fragment shader already loaded: %s", resolvedPath.c_str());
            return false;
        }
        
        if (File::Exists(resolvedPath))
        {
            m_fragmentShader = resourceManager->Load<FragmentShader>(resolvedPath, multithread);
            if (!m_fragmentShader) {
                PrintError("Failed to load Fragment Shader: %s", resolvedPath.c_str());
                return false;
            }
        }
    }
    if (auto compPath = parser["comp"].As<std::string>(); !compPath.empty())
    {
        std::filesystem::path resolvedPath = ResolvePath(compPath);
        
        if (m_computeShader)
        {
            PrintError("Compute shader already loaded: %s", resolvedPath.c_str());
            return false;
        }
        
        if (File::Exists(resolvedPath))
        {
            m_computeShader = resourceManager->Load<ComputeShader>(resolvedPath, multithread);
            if (!m_computeShader) {
                PrintError("Failed to load Compute Shader: %s", resolvedPath.c_str());
                return false;
            }
        }
    }
    if (parser.HasKey("topology"))
    {
        auto topology = topology_from_string(parser["topology"].As<std::string>());
        if (topology != Topology::None)
        {
            m_topology = topology;
        }
    }
    if (parser.HasKey("depthTest"))
        m_depthTestEnabled = (parser["depthTest"].As<std::string>() == "true");
    if (parser.HasKey("depthWrite"))
        m_depthWriteEnabled = (parser["depthWrite"].As<std::string>() == "true");
    
    if (parser.HasKey("attachmentCount"))
    {
        size_t attachmentCount = parser["attachmentCount"].As<uint64_t>();
        m_attachments.resize(attachmentCount);
        for (size_t i = 0; i < attachmentCount; ++i)
        {
            m_attachments[i] = static_cast<VkFormat>(parser["attachment " + std::to_string(i)].As<int>());
        }
    }
    bool hasGraphics = (m_vertexShader && m_fragmentShader);
    bool hasCompute = (m_computeShader.valid());

    if (!hasGraphics && !hasCompute)
    {
        PrintError("Shader %s is invalid: Must have (Vert + Frag) or (Compute)", p_path.generic_string().c_str());
        return false;
    }
    
    m_graphic = hasGraphics;

    return true;
}

bool Shader::SendToGPU(VulkanRenderer* renderer)
{
    if (m_graphic && (!m_vertexShader->HasBeenSent() || !m_fragmentShader->HasBeenSent()) 
        || !m_graphic && (!m_computeShader->HasBeenSent()))
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
    IResource::Unload();
    m_vertexShader.reset();
    m_fragmentShader.reset();
    m_computeShader.reset();
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
