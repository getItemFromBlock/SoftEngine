#pragma once
#include <memory>
#include <vector>

#include "RHIIndexBuffer.h"
#include "RHIPipeline.h"
#include "RHIShaderBuffer.h"
#include "RHITexture.h"
#include "RHIVertexBuffer.h"
#include "Utils/Type.h"

class VertexShader;
class FragmentShader;
class Shader;
struct Uniform;
enum class ShaderType;
class Texture;
class Window;

namespace ImageLoader
{
    struct Image;
}

enum class RenderAPI
{
    None,
    Vulkan,
    DirectX,
    OpenGL,
    Metal,
};

class RHIRenderer
{
public:
    RHIRenderer() = default;
    virtual ~RHIRenderer();

    virtual bool IsInitialized() const { return p_initialized; }

    static std::unique_ptr<RHIRenderer> Create(RenderAPI api, Window* window);

    virtual bool Initialize(Window* window) = 0;
    virtual void WaitForGPU() = 0;
    virtual void Cleanup() = 0;
    
    virtual void WaitUntilFrameFinished() = 0;
    virtual bool BeginFrame() = 0;
    virtual void Update() = 0;
    virtual void EndFrame() = 0;
    virtual bool MultiThreadSendToGPU() = 0;
    virtual void ClearColor() const = 0;
    virtual void DrawVertex(RHIVertexBuffer* _vertexBuffer, RHIIndexBuffer* indexBuffer) = 0;
    
    virtual void DrawFrame() {}
    
    virtual std::unique_ptr<RHITexture> CreateTexture(const ImageLoader::Image& image) = 0;
    virtual std::unique_ptr<RHIVertexBuffer> CreateVertexBuffer(const float* data, uint32_t size, uint32_t floatPerVertex) = 0;
    virtual std::unique_ptr<RHIIndexBuffer> CreateIndexBuffer(const uint32_t* data, uint32_t size) = 0;
    virtual std::unique_ptr<RHIShaderBuffer> CreateShaderBuffer(const std::string& code) = 0;
    virtual std::unique_ptr<RHIPipeline> CreatePipeline(const VertexShader* vertexShader, const FragmentShader* fragmentShader, const std::vector<Uniform>& uniforms) = 0;
    
    virtual std::string CompileShader(ShaderType type, const std::string& code) = 0;
    virtual std::vector<Uniform> GetUniforms(Shader* shader) = 0;
    virtual void SendValue(void* value, uint32_t size, Shader* shader) = 0;
    virtual void BindShader(Shader* shader) = 0;
    
    virtual void SetDefaultTexture(const SafePtr<Texture>& texture) = 0;
    
protected:
    RenderAPI p_renderAPI;
    bool p_initialized = false;
    
};
