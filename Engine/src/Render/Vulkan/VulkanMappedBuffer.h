#pragma once

#include <memory>
#include "VulkanBuffer.h"

class VulkanMappedBuffer
{
public:
	VulkanMappedBuffer() = default;
	~VulkanMappedBuffer();

    bool Initialize(VulkanDevice *device, VkDeviceSize size, bool autoFlush = true);
    void Cleanup();
    void UpdateData(const void* data, VkDeviceSize offset, VkDeviceSize size);
    // Only call if autoFlush was set to false when creating the buffer
    void FlushData(VkDeviceSize offset, VkDeviceSize size);
    void CopyDataToDevice(VkCommandBuffer cmd, VkDeviceSize offset, VkDeviceSize size);

    VkBuffer GetBuffer() const { return m_buffer.GetBuffer();}
    VkBuffer GetStagingBuffer() const { return m_stagingBuffer.GetBuffer();}
    VkDeviceSize GetSize() const { return m_buffer.GetSize(); }
    void *GetMappedBuffer() const { return m_mappedBuffer; }

private:
	VulkanBuffer    m_buffer;
	VulkanBuffer    m_stagingBuffer;
	void            *m_mappedBuffer = nullptr;
};
