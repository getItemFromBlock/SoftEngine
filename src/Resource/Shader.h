#pragma once
#include "IResource.h"
#include "Utils/Type.h"

class FragmentShader;
class VertexShader;

class BaseShader : public IResource
{
public:
    BaseShader(std::filesystem::path path) : IResource(std::move(path)) {}
    BaseShader(const BaseShader&) = delete;
    BaseShader(BaseShader&&) = delete;
    BaseShader& operator=(const BaseShader&) = delete;
    virtual ~BaseShader() override = default;
    
    virtual bool Load(ResourceManager* resourceManager) override;
    virtual bool SendToGPU(RHIRenderer* renderer) override;
    virtual void Unload() override {}
    
private:
};

class Shader : public IResource
{
public:
    Shader(std::filesystem::path path) : IResource(std::move(path)) {}
    Shader(const Shader&) = delete;
    Shader(Shader&&) = delete;
    Shader& operator=(const Shader&) = delete;
    virtual ~Shader() override = default;
    
    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(RHIRenderer* renderer) override;
    void Unload() override;

private:
    // Pipeline m_pipeline;
    SafePtr<VertexShader> m_vertexShader;
    SafePtr<FragmentShader> m_fragmentShader;
};
