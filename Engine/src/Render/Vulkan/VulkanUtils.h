#pragma once
#include <vulkan/vulkan_core.h>
#include "VulkanDevice.h"

namespace VulkanUtils
{
	void TransitionImageLayout(VulkanCommandPool *_commandBuffer, VulkanQueue &graphicsQueue,
		VkImageLayout oldLayout, VkImageLayout newLayout, VulkanDevice* m_device, VkImage image, uint32_t layerCount = 1);
}