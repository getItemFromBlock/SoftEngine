#pragma once
#include <vulkan/vulkan_core.h>

#include "VulkanDevice.h"

class VulkanShaderBuffer
{
public:
    VulkanShaderBuffer() {}

    bool Initialize(VulkanDevice* device, const std::string& code);
    
    void CleanUp();
    
    VkShaderModule GetModule() const { return m_module; }
private:
    VkShaderModule m_module = VK_NULL_HANDLE;
    VulkanDevice* m_device = nullptr;
};
