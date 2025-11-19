#pragma once

#ifdef RENDER_API_VULKAN

#include "VulkanBuffer.h"
#include <vulkan/vulkan.h>

#include "Render/RHI/RHIIndexBuffer.h"

class VulkanDevice;

class VulkanIndexBuffer : public RHIIndexBuffer
{
public:
    VulkanIndexBuffer() = default;
    ~VulkanIndexBuffer();

    // Initialize index buffer with index data
    bool Initialize(VulkanDevice* device, const void* indices, VkDeviceSize size, VkIndexType indexType = VK_INDEX_TYPE_UINT32, VkCommandPool
                    commandPool = nullptr);
    
    // Initialize empty index buffer (for dynamic updates)
    bool Initialize(VulkanDevice* device, VkDeviceSize size, VkIndexType indexType = VK_INDEX_TYPE_UINT32);
    
    void Cleanup();
    
    // Update index data (only works if buffer was created with HOST_VISIBLE memory)
    void UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    // Bind index buffer to command buffer
    void Bind(VkCommandBuffer commandBuffer);
    
    // Getters
    VkBuffer GetBuffer() const { return m_buffer ? m_buffer->GetBuffer() : VK_NULL_HANDLE; }
    VkDeviceSize GetSize() const { return m_size; }
    uint32_t GetIndexCount() const { return m_indexCount; }
    VkIndexType GetIndexType() const { return m_indexType; }
    
    void SetIndexCount(uint32_t count) { m_indexCount = count; }

private:
    bool CreateIndexBuffer(VulkanDevice* device, const void* indices, VkDeviceSize size, VkCommandPool commandPool);
    
private:
    VulkanDevice* m_device = nullptr;
    VulkanBuffer* m_buffer = nullptr;
    VkDeviceSize m_size = 0;
    uint32_t m_indexCount = 0;
    VkIndexType m_indexType = VK_INDEX_TYPE_UINT32;
};

#endif // RENDER_API_VULKAN