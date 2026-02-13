#include "VulkanDescriptorSet.h"
#include "VulkanDevice.h"
#include "VulkanUniformBuffer.h"
#include "VulkanTexture.h"
#include <stdexcept>
#include <iostream>
#include <ranges>

#include "Debug/Log.h"
#include "Resource/Texture.h"

VulkanDescriptorSet::~VulkanDescriptorSet()
{
    Cleanup();
}

bool VulkanDescriptorSet::Initialize(VulkanDevice* device, VkDescriptorPool pool,
                                     VkDescriptorSetLayout layout, uint32_t count)
{
    m_device = device;
    m_descriptorSets.resize(count);

    std::vector<VkDescriptorSetLayout> layouts(count, layout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(m_device->GetDevice(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
    {
        PrintError("Failed to allocate descriptor sets!");
        return false;
    }
    return true;
}

void VulkanDescriptorSet::Cleanup()
{
    m_descriptorSets.clear();
}

VkDescriptorSet VulkanDescriptorSet::GetDescriptorSet(uint32_t index) const
{
    if (index >= m_descriptorSets.size())
    {
        throw std::runtime_error("Invalid descriptor set index!");
    }
    return m_descriptorSets[index];
}