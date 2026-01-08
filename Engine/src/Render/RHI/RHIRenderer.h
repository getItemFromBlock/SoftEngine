#pragma once
#include "EngineAPI.h"
#include <memory>
#include <vector>

#include "RHIIndexBuffer.h"
#include "RHIPipeline.h"
#include "RHIShaderBuffer.h"
#include "RHITexture.h"
#include "RHIUniformBuffer.h"
#include "RHIVertexBuffer.h"
#include "Render/RenderQueue.h"
#include "Resource/Material.h"

#include "Resource/Shader.h"

#include "Utils/Type.h"

class Mesh;
class VertexShader;
class FragmentShader;
class Shader;

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

class ENGINE_API RHIRenderer
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
    
    virtual void BindVertexBuffers(RHIVertexBuffer* _vertexBuffer, RHIIndexBuffer* _indexBuffer) const = 0;
    virtual void SendPushConstants(void* data, uint32_t size, Shader* shader, PushConstant pushConstant) const = 0;
    virtual void DrawVertex(RHIVertexBuffer* _vertexBuffer, RHIIndexBuffer* indexBuffer) = 0;
    virtual void DrawVertexSubMesh(RHIIndexBuffer* _indexBuffer, uint32_t startIndex, uint32_t indexCount) {}
    virtual void DrawInstanced(RHIIndexBuffer* indexBuffer, RHIVertexBuffer* vertexShader, RHIBuffer* instanceBuffer, uint32_t instanceCount) {}
    
    virtual void DrawFrame() {}
    
    virtual std::unique_ptr<RHITexture> CreateTexture(const ImageLoader::Image& image) = 0;
    virtual std::unique_ptr<RHIVertexBuffer> CreateVertexBuffer(const float* data, uint32_t size, uint32_t floatPerVertex) = 0;
    virtual std::unique_ptr<RHIIndexBuffer> CreateIndexBuffer(const uint32_t* data, uint32_t size) = 0;
    virtual std::unique_ptr<RHIShaderBuffer> CreateShaderBuffer(const std::string& code) = 0;
    virtual std::unique_ptr<RHIPipeline> CreatePipeline(const Shader* shader) = 0;
    virtual std::unique_ptr<RHIMaterial> CreateMaterial(Shader* shader) = 0;
    virtual std::unique_ptr<ComputeDispatch> CreateDispatch(Shader* shader) = 0;
    
    virtual std::string CompileShader(ShaderType type, const std::string& code) = 0;
    virtual Uniforms GetUniforms(Shader* shader) = 0;
    virtual PushConstants GetPushConstants(Shader* shader) = 0;
    
    virtual void SendTexture(UBOBinding binding, Texture* texture, Shader* shader) = 0;
    virtual void SendValue(UBOBinding binding, void* value, uint32_t size, Shader* shader) = 0;
    virtual bool BindShader(Shader* shader) = 0;
    virtual bool BindMaterial(Material* material) = 0;
    
    virtual void SetDefaultTexture(const SafePtr<Texture>& texture) = 0;
    
    RenderQueueManager* GetRenderQueueManager() const { return m_renderQueue.get(); }
    
    uint64_t GetTriangleCount() const { return p_triangleCount; }
protected:
    RenderAPI p_renderAPI;
    bool p_initialized = false;
    std::unique_ptr<RenderQueueManager> m_renderQueue;
    uint64_t p_triangleCount = 0;
    
};