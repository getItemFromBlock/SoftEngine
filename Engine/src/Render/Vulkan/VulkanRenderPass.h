#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <xstring>

class VulkanGBuffer;
class VulkanDevice;

class VulkanRenderPass
{
public:
    ~VulkanRenderPass();

    bool Initialize(VulkanDevice* device, VkFormat swapChainImageFormat);
    void Cleanup();
    
    void Begin(VkCommandBuffer commandBuffer, VkImageView colorImageView,
               VkImageView depthImageView, VkExtent2D extent,
               const std::vector<VkClearValue>& clearValues, bool clearAttachment = true);
    
    void End(VkCommandBuffer commandBuffer);

    VkFormat GetColorFormat() const;
    VkFormat GetDepthFormat() const;

    void BeginGBuffer(VkCommandBuffer commandBuffer, VulkanGBuffer* gBuffer, VkImageView depthImageView, VkExtent2D extent);
    void EndGBuffer(VkCommandBuffer commandBuffer, VulkanGBuffer* gBuffer);
    void BeginComposition(VkCommandBuffer commandBuffer, VkImageView swapchainImageView, VkExtent2D extent);

private:
    VulkanDevice* m_device = nullptr;
    VkFormat m_swapChainImageFormat = VK_FORMAT_UNDEFINED;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;
};
