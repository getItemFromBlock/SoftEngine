#include "VulkanDescriptorSetLayout.h"

VulkanDescriptorSetLayout::~VulkanDescriptorSetLayout()
{
    Cleanup();
}

void VulkanDescriptorSetLayout::Create(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindingsInfo)
{
    m_device = device;
    m_bindings = bindingsInfo;
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());
    layoutInfo.pBindings = m_bindings.data();

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_layout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }
}

void VulkanDescriptorSetLayout::Cleanup()
{
    if (m_layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
    m_bindings.clear();
}
