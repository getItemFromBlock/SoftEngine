#include "VulkanIndexBuffer.h"
#ifdef RENDER_API_VULKAN

#include "VulkanDevice.h"
#include <iostream>

#include "VulkanCommandPool.h"

VulkanIndexBuffer::~VulkanIndexBuffer()
{
    Cleanup();
}

bool VulkanIndexBuffer::Initialize(VulkanDevice* device, const void* indices, VkDeviceSize size, VkIndexType indexType, VulkanCommandPool* commandPool)
{
    m_device = device;
    m_size = size;
    m_indexType = indexType;
    
    return CreateIndexBuffer(device, indices, size, commandPool);
}

bool VulkanIndexBuffer::Initialize(VulkanDevice* device, VkDeviceSize size, VkIndexType indexType)
{
    m_device = device;
    m_size = size;
    m_indexType = indexType;
    
    m_buffer = new VulkanBuffer();
    if (!m_buffer->Initialize(device, size,
                              VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        std::cerr << "Failed to create index buffer!" << std::endl;
        delete m_buffer;
        m_buffer = nullptr;
        return false;
    }
    
    return true;
}

void VulkanIndexBuffer::Cleanup()
{
    if (m_buffer)
    {
        m_buffer->Cleanup();
        delete m_buffer;
        m_buffer = nullptr;
    }
    
    m_device = nullptr;
    m_size = 0;
    m_indexCount = 0;
}

void VulkanIndexBuffer::UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (!m_buffer)
    {
        std::cerr << "Cannot update data: index buffer not initialized!" << std::endl;
        return;
    }
    
    if (offset + size > m_size)
    {
        std::cerr << "Update data exceeds buffer size!" << std::endl;
        return;
    }
    
    void* mappedData;
    vkMapMemory(m_device->GetDevice(), m_buffer->GetBufferMemory(), offset, size, 0, &mappedData);
    memcpy(mappedData, data, static_cast<size_t>(size));
    vkUnmapMemory(m_device->GetDevice(), m_buffer->GetBufferMemory());
}

void VulkanIndexBuffer::Bind(VkCommandBuffer commandBuffer)
{
    if (!m_buffer)
    {
        std::cerr << "Cannot bind: index buffer not initialized!" << std::endl;
        return;
    }
    
    vkCmdBindIndexBuffer(commandBuffer, m_buffer->GetBuffer(), 0, m_indexType);
}

bool VulkanIndexBuffer::CreateIndexBuffer(VulkanDevice* device, const void* indices, VkDeviceSize size, VulkanCommandPool* commandPool)
{
    // Create staging buffer
    VulkanBuffer stagingBuffer;
    if (!stagingBuffer.Initialize(device, size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        std::cerr << "Failed to create staging buffer!" << std::endl;
        return false;
    }
    
    stagingBuffer.CopyData(indices, size);
    
    // Create index buffer on device local memory
    m_buffer = new VulkanBuffer();
    if (!m_buffer->Initialize(device, size,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        std::cerr << "Failed to create index buffer!" << std::endl;
        stagingBuffer.Cleanup();
        delete m_buffer;
        m_buffer = nullptr;
        return false;
    }
    
    // Copy from staging buffer to index buffer
    VkCommandBuffer commandBufferResult = device->BeginSingleTimeCommands(commandPool);
    m_buffer->CopyFrom(commandBufferResult, &stagingBuffer, size);
    device->EndSingleTimeCommands(commandPool, device->GetGraphicsQueue(), commandBufferResult);
    
    // Cleanup staging buffer
    stagingBuffer.Cleanup();
    
    return true;
}

#endif // RENDER_API_VULKAN