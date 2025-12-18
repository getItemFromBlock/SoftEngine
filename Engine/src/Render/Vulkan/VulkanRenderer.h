#pragma once
#include "EngineAPI.h"
#include <memory>

#include "VulkanDescriptorSetLayout.h"
#include "Render/RHI/RHIRenderer.h"
#include "Resource/Material.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"
#include "Utils/Type.h"

#ifdef RENDER_API_VULKAN

#include <galaxymath/Maths.h>
#include <vulkan/vulkan.h>
#include <memory>

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanFramebuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanDepthBuffer.h"
#include "VulkanDescriptorPool.h"
#include "VulkanDescriptorSet.h"
#include "VulkanSyncObjects.h"
#include "VulkanUniformBuffer.h"

enum class ShaderType;
class Window;

struct UniformBufferObject
{
    Mat4 Model;
    Mat4 View;
    Mat4 Projection;
};

class ENGINE_API VulkanRenderer : public RHIRenderer
{
public:
    VulkanRenderer() = default;
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    ~VulkanRenderer() override;

    bool Initialize(Window* window) override;
    void WaitForGPU() override;
    void Cleanup() override;
    
    void WaitUntilFrameFinished() override;
    bool BeginFrame() override;
    void Update() override;
    void EndFrame() override;
    
    void SendPushConstants(void* data, size_t size, Shader* shader, PushConstant pushConstant) const override;
    void BindVertexBuffers(RHIVertexBuffer* _vertexBuffer, RHIIndexBuffer* _indexBuffer) const override;
    void DrawVertex(RHIVertexBuffer* _vertexBuffer, RHIIndexBuffer* _indexBuffer) override;
    
    void DrawFrame() override;
    
    bool MultiThreadSendToGPU() override;

    std::string CompileShader(ShaderType type, const std::string& code) override;
    Uniforms GetUniforms(Shader* shader) override;
    PushConstants GetPushConstants(Shader* shader) override;
    
    VulkanCommandPool* GetCommandPool() const { return m_commandPool.get(); }
    
    void SendTexture(UBOBinding binding, Texture* texture, Shader* shader) override;
    void SendValue(UBOBinding binding, void* value, uint32_t size, Shader* shader) override;
    bool BindMaterial(Material* material) override;
    
    std::unique_ptr<RHITexture> CreateTexture(const ImageLoader::Image& image) override;
    std::unique_ptr<RHIVertexBuffer> CreateVertexBuffer(const float* data, uint32_t size, uint32_t floatPerVertex) override;
    std::unique_ptr<RHIIndexBuffer> CreateIndexBuffer(const uint32_t* data, uint32_t size) override;
    std::unique_ptr<RHIShaderBuffer> CreateShaderBuffer(const std::string& code) override;
    std::unique_ptr<RHIPipeline> CreatePipeline(const Shader* shader) override;
    std::unique_ptr<RHIMaterial> CreateMaterial(Shader* shader) override;
    
    void SetDefaultTexture(const SafePtr<Texture>& texture) override;
    void ClearColor() const override;
    
    uint32_t GetFrameIndex() const { return m_currentFrame; }
    VkCommandBuffer GetCommandBuffer() const { return m_commandPool->GetCommandBuffer(m_currentFrame); }
    
    VulkanContext* GetContext() const { return m_context.get(); }
    VulkanDevice* GetDevice() const { return m_device.get(); }
    VulkanRenderPass* GetRenderPass() const { return m_renderPass.get(); }
    VulkanSwapChain* GetSwapChain() const { return m_swapChain.get(); }
    
    uint32_t GetMaxFramesInFlight() const { return MAX_FRAMES_IN_FLIGHT; }
private:
    void RecreateSwapChain();

private:
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
    VkDescriptorPool m_imGuiPool;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t m_currentFrame = 0;
};

#endif