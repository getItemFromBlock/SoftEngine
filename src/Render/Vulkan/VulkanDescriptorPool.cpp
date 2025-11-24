#include "VulkanDescriptorPool.h"
#include "VulkanDevice.h"
#include <stdexcept>
#include <iostream>

#include "Debug/Log.h"

VulkanDescriptorPool::~VulkanDescriptorPool()
{
    Cleanup();
}

bool VulkanDescriptorPool::Initialize(VulkanDevice* device, const std::vector<VkDescriptorPoolSize>& poolSizes,
                                      uint32_t maxSets)
{
    m_device = device;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;
    poolInfo.flags = 0; // Optional: VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT

    if (vkCreateDescriptorPool(m_device->GetDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        PrintError("Failed to create descriptor pool!");
        return false;
    }

    PrintLog("Descriptor pool created successfully!");
    return true;
}

void VulkanDescriptorPool::Cleanup()
{
    if (m_descriptorPool != VK_NULL_HANDLE && m_device)
    {
        vkDestroyDescriptorPool(m_device->GetDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
        PrintLog("Descriptor pool destroyed");
    }
}
