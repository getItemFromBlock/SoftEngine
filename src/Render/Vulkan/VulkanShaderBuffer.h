#pragma once

#include "Render/RHI/RHIShaderBuffer.h"

#ifdef RENDER_API_VULKAN
#include <vulkan/vulkan_core.h>

#include "VulkanDevice.h"

class VulkanShaderBuffer : public RHIShaderBuffer
{
public:
    VulkanShaderBuffer() : RHIShaderBuffer() {}

    bool Initialize(VulkanDevice* device, const std::string& code);
    
    void CleanUp() override;
    
    VkShaderModule GetModule() const { return m_module; }
private:
    VkShaderModule m_module = VK_NULL_HANDLE;
    VulkanDevice* m_device = nullptr;
};

#endif
