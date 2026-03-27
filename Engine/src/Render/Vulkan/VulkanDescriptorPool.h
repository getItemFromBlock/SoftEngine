#pragma once
#include "VulkanDevice.h"

class VulkanDescriptorPool
{
public:
    VulkanDescriptorPool() = default;
    ~VulkanDescriptorPool();
    
    bool Initialize(VulkanDevice* device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags flags = 0);
    void Cleanup();
    
    VkDescriptorSet Allocate(VkDescriptorSetLayout layout);
    void Reset();

    VkDescriptorPool GetPool() const { return m_descriptorPool; }

private:
    VulkanDevice* m_device = nullptr;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
};