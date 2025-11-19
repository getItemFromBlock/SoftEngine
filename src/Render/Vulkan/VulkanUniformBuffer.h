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
    ~VulkanUniformBuffer();

    // Initialize uniform buffer with size and number of frames (per-frame buffers)
    // Creates `frameCount` buffers each of size `size`.
    bool Initialize(VulkanDevice* device, VkDeviceSize size, uint32_t frameCount);

    void Cleanup();

    // Update uniform data:
    // - Broadcast version: writes to every frame buffer.
    void UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    // - Per-frame version: writes to the specific frame buffer.
    void UpdateData(const void* data, VkDeviceSize size, uint32_t currentFrame, VkDeviceSize offset = 0);

    // Map a specific frame's memory (persistent mapping). Returns the mapped pointer.
    // If already mapped returns the existing pointer.
    void* Map(uint32_t currentFrame);

    // Map all frames persistently (useful at init).
    bool MapAll();

    // Unmap a specific frame (only if mapped).
    void Unmap(uint32_t currentFrame);

    // Unmap all frames.
    void UnmapAll();

    // Write data to an already mapped frame memory (must call Map(frame) first).
    void WriteToMapped(const void* data, VkDeviceSize size, uint32_t currentFrame, VkDeviceSize offset = 0);

    // Getters
    VkBuffer GetBuffer(size_t i) const;
    VkDeviceSize GetSize() const { return m_size; }
    uint32_t GetFrameCount() const { return static_cast<uint32_t>(m_buffer.size()); }

    // Descriptor info (per-frame)
    VkDescriptorBufferInfo GetDescriptorInfo(size_t frame) const;
    VkDescriptorBufferInfo GetDescriptorInfo(size_t frame, VkDeviceSize offset, VkDeviceSize range) const;

private:
    VulkanDevice* m_device = nullptr;
    std::vector<VulkanBuffer*> m_buffer;        // one buffer per frame
    std::vector<void*> m_mappedMemory;          // mapped pointer per frame (or nullptr)
    VkDeviceSize m_size = 0;
};

#endif // RENDER_API_VULKAN
