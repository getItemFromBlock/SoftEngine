#pragma once
#include "Render/RHI/RHIVertexBuffer.h"

#ifdef RENDER_API_VULKAN

#include "VulkanBuffer.h"
#include <vulkan/vulkan.h>

class VulkanDevice;

class VulkanVertexBuffer : public RHIVertexBuffer
{
public:
    VulkanVertexBuffer() = default;
    ~VulkanVertexBuffer() override;

    bool Initialize(VulkanDevice* device, const void* vertices, VkDeviceSize size, VkCommandPool commandPool);
    
    bool Initialize(VulkanDevice* device, VkDeviceSize size);
    
    void Cleanup();
    
    void UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    
    void Bind(VkCommandBuffer commandBuffer, uint32_t binding = 0);
    
    VkBuffer GetBuffer() const { return m_buffer ? m_buffer->GetBuffer() : VK_NULL_HANDLE; }
    VkDeviceSize GetSize() const { return m_size; }
    uint32_t GetVertexCount() const { return m_vertexCount; }
    
    void SetVertexCount(uint32_t count) { m_vertexCount = count; }

private:
    bool CreateVertexBuffer(VulkanDevice* device, const void* vertices, VkDeviceSize size, VkCommandPool commandPool);
    
private:
    VulkanDevice* m_device = nullptr;
    VulkanBuffer* m_buffer = nullptr;
    VkDeviceSize m_size = 0;
    uint32_t m_vertexCount = 0;
};

#endif // RENDER_API_VULKAN