#pragma once
#include "IResource.h"
#include "Utils/Type.h"

#include "Render/RHI/RHIPipeline.h"
#include "Render/RHI/RHIShaderBuffer.h"
#include "Render/RHI/RHIUniformBuffer.h"

class ComputeDispatch;
class Texture;
class FragmentShader;
class VertexShader;
class ComputeShader;

enum class ShaderType
{
    None,
    Vertex,
    Fragment,
    Compute
};

class BaseShader : public IResource
{
public:
    BaseShader(std::filesystem::path path) : IResource(std::move(path)) {}
    BaseShader(const BaseShader&) = delete;
    BaseShader(BaseShader&&) = delete;
    BaseShader& operator=(const BaseShader&) = delete;
    virtual ~BaseShader() override;
    
    virtual bool Load(ResourceManager* resourceManager) override;
    virtual bool SendToGPU(RHIRenderer* renderer) override;
    virtual void Unload() override {}
    
    virtual ResourceType GetResourceType() const override = 0;
    
    virtual ShaderType GetShaderType() const = 0;
    std::string GetContent() const { return p_content; }
    RHIShaderBuffer* GetBuffer() const { return p_buffer.get(); }
    
    std::filesystem::path GetCompiledPath() const;
private:
    std::string p_content;
    bool p_compiled = false;
    std::unique_ptr<RHIShaderBuffer> p_buffer;
};

enum class UniformType
{
    None,
    Unknown,
    Float,
    Int,
    UInt,
    Bool,
    Vec2,
    Vec3,
    Vec4,
    IVec2,
    IVec3,
    IVec4,
    Mat2,
    Mat3,
    Mat4,
    NestedStruct,
    StorageBuffer,
    Sampler2D,
    SamplerCube
};

struct UniformMember {
    std::string name;
    std::string typeName;
    UniformType type = UniformType::None;
    uint32_t offset = 0;
    uint32_t size = 0;
    bool isArray = false;
    std::vector<uint32_t> arrayDims;
    std::vector<UniformMember> members;
};

struct Uniform {
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
    uint32_t size = 0;
    UniformType type = UniformType::None;
    std::vector<UniformMember> members;
    ShaderType shaderType = ShaderType::None;
};

struct PushConstant
{
    std::string name;   
    uint32_t size = 0;  
    uint32_t offset = 0;
    ShaderType shaderType = ShaderType::None;
    std::vector<UniformMember> members;
};

using Uniforms = std::unordered_map<std::string, Uniform>;
using PushConstants = std::unordered_map<ShaderType, PushConstant>;
class Shader : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(Shader)
    
    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(RHIRenderer* renderer) override;
    void Unload() override;

    PushConstants GetPushConstants() const {return m_pushConstants;}
    Uniform GetUniform(const std::string& name) { return m_uniforms[name]; }
    Uniforms GetUniforms() const { return m_uniforms; }
    
    VertexShader* GetVertexShader() const { return m_vertexShader.getPtr(); }
    FragmentShader* GetFragmentShader() const { return m_fragmentShader.getPtr(); }
    ComputeShader* GetComputeShader() const { return m_computeShader.getPtr(); }
    
    void SendTexture(UBOBinding binding, Texture* texture, RHIRenderer* renderer);
    void SendValue(UBOBinding binding, void* value, uint32_t size, RHIRenderer* renderer);
    
    RHIPipeline* GetPipeline() const { return m_pipeline.get(); }
    
    bool IsGraphic() const { return m_graphic; }
    
    std::unique_ptr<ComputeDispatch> CreateDispatch(RHIRenderer* renderer);
private:
private:
    SafePtr<VertexShader> m_vertexShader;
    SafePtr<FragmentShader> m_fragmentShader;
    SafePtr<ComputeShader> m_computeShader;

    PushConstants m_pushConstants;
    Uniforms m_uniforms;
    std::unique_ptr<RHIPipeline> m_pipeline;
    
    bool m_graphic;
};
