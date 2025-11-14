#pragma once
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>

class VulkanDevice;

class VulkanRenderPass
{
public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass() = default;

    bool Initialize(VulkanDevice* device, VkFormat swapChainImageFormat);
    void Cleanup();

    VkRenderPass GetRenderPass() const { return m_renderPass; }

    void Begin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, 
               VkExtent2D extent, VkClearValue clearValue);
    void End(VkCommandBuffer commandBuffer);

private:
    VulkanDevice* m_device = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};

#endif