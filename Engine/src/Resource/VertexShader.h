#pragma once
#include "IResource.h"

#include "Shader.h"

class VertexShader : public BaseShader
{
public:
    VertexShader(std::filesystem::path path) : BaseShader(std::move(path)) {}
    VertexShader(const VertexShader&) = delete;
    VertexShader(VertexShader&&) = delete;
    VertexShader& operator=(const VertexShader&) = delete;
    virtual ~VertexShader() override = default;
    
    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(VulkanRenderer* renderer) override;
    void Unload() override;
    
    ResourceType GetResourceType() const override { return ResourceType::VertexShader; }
    
    ShaderType GetShaderType() const override { return ShaderType::Vertex; }
};
