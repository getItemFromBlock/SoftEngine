#pragma once

#include "VulkanBuffer.h"
#include <vulkan/vulkan.h>

class VulkanCommandPool;
class VulkanDevice;

class VulkanVertexBuffer
{
public:
    VulkanVertexBuffer() = default;
    ~VulkanVertexBuffer();

    bool Initialize(VulkanDevice* device, const void* vertices, VkDeviceSize size, VulkanCommandPool* commandBuffer);
    
    bool Initialize(VulkanDevice* device, VkDeviceSize size);
    
    void Cleanup();
    
    void UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    void Bind(VkCommandBuffer commandBuffer, uint32_t binding = 0);
    
    VkBuffer GetBuffer() const { return m_buffer ? m_buffer->GetBuffer() : VK_NULL_HANDLE; }
    VkDeviceSize GetSize() const { return m_size; }
    uint32_t GetVertexCount() const { return m_vertexCount; }
    
    void SetVertexCount(uint32_t count) { m_vertexCount = count; }

private:
    bool CreateVertexBuffer(VulkanDevice* device, const void* vertices, VkDeviceSize size, VulkanCommandPool* commandPool);
    
private:
    VulkanDevice* m_device = nullptr;
    VulkanBuffer* m_buffer = nullptr;
    VkDeviceSize m_size = 0;
    uint32_t m_vertexCount = 0;
};