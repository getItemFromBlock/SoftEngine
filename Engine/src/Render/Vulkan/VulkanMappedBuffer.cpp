#include "VulkanMappedBuffer.h"

#include "VulkanDevice.h"
#include "Debug/Log.h"

VulkanMappedBuffer::~VulkanMappedBuffer()
{
}

bool VulkanMappedBuffer::Initialize(VulkanDevice *device, VkDeviceSize size, bool autoFlush, bool reverseDirection, bool autoMap)
{
    ASSERT(m_buffer.GetSize() == 0 && size > 0);
    bool result = true;
    result &= m_buffer.Initialize(device, size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result &= m_stagingBuffer.Initialize(device, size,
        reverseDirection ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        autoFlush ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    if (autoMap)
        result &= vkMapMemory(device->GetDevice(), m_stagingBuffer.GetBufferMemory(), 0, VK_WHOLE_SIZE, 0, &m_mappedBuffer) != VK_SUCCESS;

    return result;
}

bool VulkanMappedBuffer::ManualMap(VulkanDevice *device)
{
    ASSERT(m_buffer.GetSize() > 0);
    ASSERT(m_mappedBuffer == nullptr);

    return vkMapMemory(device->GetDevice(), m_stagingBuffer.GetBufferMemory(), 0, VK_WHOLE_SIZE, 0, &m_mappedBuffer) != VK_SUCCESS;
}

void VulkanMappedBuffer::ManualUnmap(VulkanDevice *device)
{
    ASSERT(m_buffer.GetSize() > 0);
    ASSERT(m_mappedBuffer != nullptr);

    vkUnmapMemory(device->GetDevice(), m_stagingBuffer.GetBufferMemory());
    m_mappedBuffer = nullptr;
}

void VulkanMappedBuffer::Cleanup()
{
    if (m_buffer.GetSize())
        vkUnmapMemory(m_stagingBuffer.GetDevice()->GetDevice(), m_stagingBuffer.GetBufferMemory());

    m_stagingBuffer.Cleanup();
    m_buffer.Cleanup();
}

void VulkanMappedBuffer::UpdateData(const void *data, VkDeviceSize offset, VkDeviceSize size)
{
    ASSERT(offset + size <= m_buffer.GetSize());
    ASSERT(m_mappedBuffer != nullptr);

    std::memcpy(reinterpret_cast<uint8_t*>(m_mappedBuffer) + offset, data, size);
}

void VulkanMappedBuffer::FlushData(VkDeviceSize offset, VkDeviceSize size)
{
    ASSERT(offset + size <= m_stagingBuffer.GetSize());

    VkMappedMemoryRange stagingRange = {};
    stagingRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    stagingRange.memory = m_stagingBuffer.GetBufferMemory();
    stagingRange.offset = offset;
    stagingRange.size = size;

    vkFlushMappedMemoryRanges(m_stagingBuffer.GetDevice()->GetDevice(), 1, &stagingRange);
}

void VulkanMappedBuffer::InvalidateData(VkDeviceSize offset, VkDeviceSize size)
{
    ASSERT(offset + size <= m_stagingBuffer.GetSize());

    VkMappedMemoryRange stagingRange = {};
    stagingRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    stagingRange.memory = m_stagingBuffer.GetBufferMemory();
    stagingRange.offset = offset;
    stagingRange.size = size;

    vkInvalidateMappedMemoryRanges(m_stagingBuffer.GetDevice()->GetDevice(), 1, &stagingRange);
}

void VulkanMappedBuffer::CopyDataToDevice(VkCommandBuffer cmd, VkDeviceSize offset, VkDeviceSize size)
{
    ASSERT(offset + size <= m_buffer.GetSize());

    m_buffer.CopyFrom(cmd, &m_stagingBuffer, size, offset);

    VkBufferMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.buffer = m_buffer.GetBuffer();
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanMappedBuffer::CopyDataToCPU(VkCommandBuffer cmd, VkDeviceSize offset, VkDeviceSize size)
{
    ASSERT(offset + size <= m_buffer.GetSize());

    m_stagingBuffer.CopyFrom(cmd, &m_buffer, size, offset);
}
