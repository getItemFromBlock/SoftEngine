#pragma once
#include "EngineAPI.h"
#include <memory>

#include "Resource/Material.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"
#include "Resource/Shader.h"

#include "Utils/Type.h"

#include <galaxymath/Maths.h>

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanFramebuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanDepthBuffer.h"
#include "VulkanIndexBuffer.h"
#include "VulkanSwapChain.h"
#include "VulkanSyncObjects.h"
#include "VulkanVertexBuffer.h"
#include "Render/LineRenderer.h"
#include "Render/RenderQueue.h"

enum class ShaderType;
class Window;

struct UniformBufferObject
{
    Mat4 Model;
    Mat4 View;
    Mat4 Projection;
};

class ENGINE_API VulkanRenderer
{
public:
    VulkanRenderer() = default;
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    ~VulkanRenderer();

    bool Initialize(Window* window);
    bool IsInitialized() const { return m_initialized; }
    void WaitForGPU();
    void Cleanup();
    
    void WaitUntilFrameFinished();
    bool BeginFrame();
    void Update();
    void EndFrame();
    
    void SendPushConstants(void* data, uint32_t size, Shader* shader, PushConstant pushConstant) const;
    void BindVertexBuffers(VulkanVertexBuffer* vertexBuffer, VulkanIndexBuffer* indexBuffer) const;
    void DrawVertex(VulkanVertexBuffer* vertexBuffer, const VulkanIndexBuffer* indexBuffer);
    void DrawVertexSubMesh(VulkanIndexBuffer* _indexBuffer, uint32_t startIndex, uint32_t indexCount);
    void DrawInstanced(VulkanIndexBuffer* indexBuffer, VulkanVertexBuffer* vertexShader, VulkanBuffer* instanceBuffer, uint32_t instanceCount);
    
    void DrawFrame();
    
    bool MultiThreadSendToGPU();

    std::string CompileShader(ShaderType type, const std::string& code);
    Uniforms GetUniforms(Shader* shader);
    PushConstants GetPushConstants(Shader* shader);
    
    void SendTexture(UBOBinding binding, Texture* texture, Shader* shader);
    void SendValue(UBOBinding binding, void* value, uint32_t size, Shader* shader);
    bool BindShader(Shader* shader);
    bool BindMaterial(Material* material);
    
    std::unique_ptr<VulkanTexture> CreateTexture(const ImageLoader::Image& image);
    std::unique_ptr<VulkanVertexBuffer> CreateVertexBuffer(const float* data, uint32_t size, uint32_t floatPerVertex);
    std::unique_ptr<VulkanIndexBuffer> CreateIndexBuffer(const uint32_t* data, uint32_t size);
    std::unique_ptr<VulkanShaderBuffer> CreateShaderBuffer(const std::string& code);
    std::unique_ptr<VulkanPipeline> CreatePipeline(const Shader* shader);
    std::unique_ptr<VulkanMaterial> CreateMaterial(Shader* shader);
    std::unique_ptr<ComputeDispatch> CreateDispatch(Shader* shader);
    
    void SetDefaultTexture(const SafePtr<Texture>& texture);
    void ClearColor() const;
    
    uint32_t GetFrameIndex() const { return m_currentFrame; }
    VkCommandBuffer GetCommandBuffer() const { return m_commandPool->GetCommandBuffer(m_currentFrame); }
    
    VulkanContext* GetContext() const { return m_context.get(); }
    VulkanDevice* GetDevice() const { return m_device.get(); }
    VulkanCommandPool* GetCommandPool() const { return m_commandPool.get(); }
    VulkanRenderPass* GetRenderPass() const { return m_renderPass.get(); }
    VulkanSwapChain* GetSwapChain() const { return m_swapChain.get(); }
    VulkanSyncObjects* GetSyncObjects() const { return m_syncObjects.get(); }
    
    uint32_t GetMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }
    
    RenderQueueManager* GetRenderQueueManager() const { return m_renderQueueManager.get(); }
    uint64_t GetTriangleCount() const { return p_triangleCount; }
    uint64_t GetVertexCount() const { return p_vertexCount; }

    LineRenderer* GetLineRenderer() { return &m_lineRenderer; }
    void AddLine(const Vec3f& start, const Vec3f& end, const Vec4f& color, float thickness = 1.f);
private:
    void RecreateSwapChain();
    void TransitionImageForPresent() const;

private:
    bool m_initialized = false;
    std::unique_ptr<RenderQueueManager> m_renderQueueManager;
    uint64_t p_triangleCount = 0;
    uint64_t p_vertexCount = 0;
    
    Window* m_window = nullptr;
    bool m_framebufferResized = false;

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanSwapChain> m_swapChain;
    std::unique_ptr<VulkanRenderPass> m_renderPass;
    std::unique_ptr<VulkanFramebuffer> m_framebuffer;
    std::unique_ptr<VulkanCommandPool> m_commandPool;
    std::unique_ptr<VulkanSyncObjects> m_syncObjects;
    std::unique_ptr<VulkanDepthBuffer> m_depthBuffer;

    uint32_t m_imageIndex = 0;
    
    SafePtr<Texture> m_defaultTexture;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t m_currentFrame = 0;
    
    LineRenderer m_lineRenderer;
};