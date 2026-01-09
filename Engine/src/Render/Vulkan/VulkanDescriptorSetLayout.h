#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

class VulkanDescriptorSetLayout
{
public:
    VulkanDescriptorSetLayout(VkDevice device)
        : m_device(device), m_layout(VK_NULL_HANDLE) {}

    ~VulkanDescriptorSetLayout();

    void Create(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindingsInfo);

    void Cleanup();

    VkDescriptorSetLayout GetLayout() const { return m_layout; }

private:
    VkDevice m_device;
    VkDescriptorSetLayout m_layout;
    std::vector<VkDescriptorSetLayoutBinding> m_bindings;
};

