#pragma once

#include "Render/RHI/RHIUniformBuffer.h"

#ifdef RENDER_API_VULKAN

#include <vector>
#include <cstdint>
#include <vulkan/vulkan.h>

class VulkanDevice;
class VulkanBuffer;

class VulkanUniformBuffer : public RHIUniformBuffer
{
public:
    VulkanUniformBuffer() = default;
    ~VulkanUniformBuffer() override;

    bool Initialize(VulkanDevice* device, VkDeviceSize size, 
        uint32_t frameCount, VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    void Cleanup();

    void UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    void UpdateData(const void* data, VkDeviceSize size, uint32_t currentFrame, VkDeviceSize offset = 0);

    void* Map(uint32_t currentFrame);

    bool MapAll();

    void Unmap(uint32_t currentFrame);

    void UnmapAll();
    
    void WriteToMapped(const void* data, VkDeviceSize size, uint32_t currentFrame, VkDeviceSize offset = 0);

    VkBuffer GetBuffer(size_t i) const;
    VkDeviceSize GetSize() const { return m_size; }
    uint32_t GetFrameCount() const { return static_cast<uint32_t>(m_buffer.size()); }

    VkDescriptorBufferInfo GetDescriptorInfo(size_t frame) const;
    VkDescriptorBufferInfo GetDescriptorInfo(size_t frame, VkDeviceSize offset, VkDeviceSize range) const;

private:
    VulkanDevice* m_device = nullptr;
    std::vector<VulkanBuffer*> m_buffer;     
    std::vector<void*> m_mappedMemory;       
    VkDeviceSize m_size = 0;
};

#endif // RENDER_API_VULKAN
