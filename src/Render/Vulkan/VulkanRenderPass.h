#pragma once
#ifdef RENDER_API_VULKAN

#include <vector>
#include <vulkan/vulkan.h>

class VulkanDevice;

class VulkanRenderPass
{
public:
    VulkanRenderPass() = default;
    ~VulkanRenderPass();

    bool Initialize(VulkanDevice* device, VkFormat swapChainImageFormat);
    void Cleanup();

    VkRenderPass GetRenderPass() const { return m_renderPass; }

    void Begin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, 
               VkExtent2D extent, const std::vector<VkClearValue>& clearValues);
    void End(VkCommandBuffer commandBuffer);

    VkFormat GetSwapChainImageFormat() const { return m_swapChainImageFormat; }
private:
    VulkanDevice* m_device = nullptr;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkFormat m_swapChainImageFormat = VK_FORMAT_UNDEFINED;
};

#endif