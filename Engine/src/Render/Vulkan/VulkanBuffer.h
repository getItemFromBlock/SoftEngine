#pragma once
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <memory>

#include "Render/RHI/RHIBuffer.h"

class VulkanDevice;

class VulkanBuffer : public RHIBuffer
{
public:
    VulkanBuffer() = default;
    ~VulkanBuffer() override;

    bool Initialize(VulkanDevice* device, VkDeviceSize size, 
                   VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    void Cleanup() override;

    void CopyData(const void* data, VkDeviceSize size);
    void CopyFrom(VkCommandBuffer commandBuffer, VulkanBuffer* srcBuffer, VkDeviceSize size);

    VkDeviceMemory GetBufferMemory() const { return m_bufferMemory; }
    VkBuffer GetBuffer() const { return m_buffer; }
    VkDeviceSize GetSize() const { return m_size; }

private:
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    VulkanDevice* m_device = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_bufferMemory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
};

#endif