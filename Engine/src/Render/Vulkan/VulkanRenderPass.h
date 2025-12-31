#pragma once
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <vector>

class VulkanDevice;

class VulkanRenderPass
{
public:
    ~VulkanRenderPass();

    bool Initialize(VulkanDevice* device, VkFormat swapChainImageFormat);
    void Cleanup();
    
    void Begin(VkCommandBuffer commandBuffer, VkImageView colorImageView, 
               VkImageView depthImageView, VkExtent2D extent, 
               const std::vector<VkClearValue>& clearValues);
    
    void End(VkCommandBuffer commandBuffer);

    VkFormat GetColorFormat() const;
    VkFormat GetDepthFormat() const;

    VkRenderPass GetRenderPass() const { return VK_NULL_HANDLE; }

private:
    VulkanDevice* m_device = nullptr;
    VkFormat m_swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
};

#endif