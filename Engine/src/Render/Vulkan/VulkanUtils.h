#pragma once
#include <vulkan/vulkan_core.h>
#include "VulkanDevice.h"

class VulkanGBuffer;

namespace VulkanUtils
{
    void TransitionImageLayout(VulkanCommandPool* _commandBuffer, VulkanQueue& graphicsQueue,
                               VkImageLayout oldLayout, VkImageLayout newLayout, VulkanDevice* m_device, VkImage image,
                               uint32_t layerCount = 1);


    void TransitionGBufferToColorAttachment(VkCommandBuffer commandBuffer, VulkanGBuffer& gBuffer);
    void TransitionGBufferToShaderRead(VkCommandBuffer commandBuffer, VulkanGBuffer& gBuffer);
}
