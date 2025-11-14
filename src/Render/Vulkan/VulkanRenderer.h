#pragma once
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <memory>

#include "Render/RHI/RHIRenderer.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanFramebuffer.h"
#include "VulkanCommandBuffer.h"
#include "VulkanSyncObjects.h"

class Window;

class VulkanRenderer : public RHIRenderer
{
public:
    VulkanRenderer() = default;
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;
    VulkanRenderer(VulkanRenderer&&) = delete;
    ~VulkanRenderer() override;

    bool Initialize(Window* window) override;
    void Cleanup() override;

    void BeginFrame();
    void EndFrame();
    void DrawFrame();

    bool IsInitialized() const { return m_initialized; }

private:
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void RecreateSwapChain();

    Window* m_window = nullptr;
    bool m_initialized = false;
    bool m_framebufferResized = false;

    std::unique_ptr<VulkanContext> m_context;
    std::unique_ptr<VulkanDevice> m_device;
    std::unique_ptr<VulkanSwapChain> m_swapChain;
    std::unique_ptr<VulkanRenderPass> m_renderPass;
    std::unique_ptr<VulkanPipeline> m_pipeline;
    std::unique_ptr<VulkanFramebuffer> m_framebuffer;
    std::unique_ptr<VulkanCommandBuffer> m_commandBuffer;
    std::unique_ptr<VulkanSyncObjects> m_syncObjects;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    uint32_t m_currentFrame = 0;
};

#endif