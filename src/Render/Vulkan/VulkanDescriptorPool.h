#pragma once
#ifdef RENDER_API_VULKAN
#include "VulkanDevice.h"

class VulkanDescriptorPool
{
public:
    VulkanDescriptorPool() = default;
    ~VulkanDescriptorPool();

    bool Initialize(VulkanDevice* device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets);
    void Cleanup();

    VkDescriptorPool GetPool() const { return m_descriptorPool; }

private:
    VulkanDevice* m_device = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
};
#endif