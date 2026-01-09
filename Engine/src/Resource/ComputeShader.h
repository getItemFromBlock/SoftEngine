#pragma once
#include "Shader.h"
#include "Render/Vulkan/VulkanMaterial.h"

class ComputeDispatch
{
public:
    ComputeDispatch() = default;
    ComputeDispatch(const ComputeDispatch&) = delete;
    ComputeDispatch(ComputeDispatch&&) = delete;
    ComputeDispatch& operator=(const ComputeDispatch&) = delete;
    virtual ~ComputeDispatch();
    
    void SetMaterial(std::unique_ptr<VulkanMaterial> mat) { m_buffer = std::move(mat); }

    VulkanMaterial* GetMaterial() const { return m_buffer.get(); }
private:
    std::unique_ptr<VulkanMaterial> m_buffer;
};

class ComputeShader : public BaseShader
{
public:
    ComputeShader(std::filesystem::path path) : BaseShader(std::move(path)) {}
    ComputeShader(const ComputeShader&) = delete;
    ComputeShader(ComputeShader&&) = delete;
    ComputeShader& operator=(const ComputeShader&) = delete;
    virtual ~ComputeShader() override {}
    
    ResourceType GetResourceType() const override { return ResourceType::ComputeShader; }
    
    ShaderType GetShaderType() const override { return ShaderType::Compute; }
    
    bool SendToGPU(VulkanRenderer* renderer) override;
};
