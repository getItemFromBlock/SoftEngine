#include "VulkanDescriptorPool.h"
#include "VulkanDevice.h"
#include <stdexcept>
#include <iostream>

#include "Debug/Log.h"

VulkanDescriptorPool::~VulkanDescriptorPool()
{
    Cleanup();
}

bool VulkanDescriptorPool::Initialize(VulkanDevice* device, const std::vector<VkDescriptorPoolSize>& poolSizes, uint32_t maxSets, VkDescriptorPoolCreateFlags flags)
{
    m_device = device;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;
    poolInfo.flags = flags;

    if (vkCreateDescriptorPool(m_device->GetDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
    {
        PrintError("Failed to create descriptor pool!");
        return false;
    }
    return true;
}


void VulkanDescriptorPool::Cleanup()
{
    if (m_descriptorPool != VK_NULL_HANDLE && m_device)
    {
        vkDestroyDescriptorPool(m_device->GetDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
}

VkDescriptorSet VulkanDescriptorPool::Allocate(VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(m_device->GetDevice(), &allocInfo, &set) != VK_SUCCESS)
    {
        PrintError("Failed to allocate descriptor set from pool!");
        return VK_NULL_HANDLE;
    }
    return set;
}

void VulkanDescriptorPool::Reset()
{
    if (m_descriptorPool != VK_NULL_HANDLE && m_device)
        vkResetDescriptorPool(m_device->GetDevice(), m_descriptorPool, 0);
}
