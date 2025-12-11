#pragma once
#include "Shader.h"

class FragmentShader : public BaseShader
{
public:
    FragmentShader(std::filesystem::path path) : BaseShader(std::move(path)) {}
    FragmentShader(const FragmentShader&) = delete;
    FragmentShader(FragmentShader&&) = delete;
    FragmentShader& operator=(const FragmentShader&) = delete;
    virtual ~FragmentShader() override = default;
    
    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(RHIRenderer* renderer) override;
    void Unload() override;
    
    ResourceType GetResourceType() const override { return ResourceType::VertexShader; }
    
    ShaderType GetShaderType() const override { return ShaderType::Fragment; }
private:
};

