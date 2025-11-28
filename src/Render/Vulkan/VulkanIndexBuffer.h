#pragma once

#ifdef RENDER_API_VULKAN

#include "VulkanBuffer.h"
#include <vulkan/vulkan.h>

#include "Render/RHI/RHIIndexBuffer.h"

class VulkanCommandPool;
class VulkanDevice;

class VulkanIndexBuffer : public RHIIndexBuffer
{
public:
    VulkanIndexBuffer() = default;
    ~VulkanIndexBuffer();

    bool Initialize(VulkanDevice* device, const void* indices, VkDeviceSize size,
                    VkIndexType indexType = VK_INDEX_TYPE_UINT32, VulkanCommandPool* commandPool = nullptr);

    bool Initialize(VulkanDevice* device, VkDeviceSize size, VkIndexType indexType = VK_INDEX_TYPE_UINT32);

    void Cleanup();

    void UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    void Bind(VkCommandBuffer commandBuffer);

    // Getters
    VkBuffer GetBuffer() const { return m_buffer ? m_buffer->GetBuffer() : VK_NULL_HANDLE; }
    VkDeviceSize GetSize() const { return m_size; }
    uint32_t GetIndexCount() const { return m_indexCount; }
    VkIndexType GetIndexType() const { return m_indexType; }

    void SetIndexCount(uint32_t count) { m_indexCount = count; }

private:
    bool CreateIndexBuffer(VulkanDevice* device, const void* indices, VkDeviceSize size, VulkanCommandPool* commandPool);

private:
    VulkanDevice* m_device = nullptr;
    VulkanBuffer* m_buffer = nullptr;
    VkDeviceSize m_size = 0;
    uint32_t m_indexCount = 0;
    VkIndexType m_indexType = VK_INDEX_TYPE_UINT32;
};

#endif // RENDER_API_VULKAN
