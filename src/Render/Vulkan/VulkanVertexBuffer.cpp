#include "VulkanVertexBuffer.h"
#ifdef RENDER_API_VULKAN

#include "VulkanDevice.h"
#include <iostream>

VulkanVertexBuffer::~VulkanVertexBuffer()
{
    Cleanup();
}

bool VulkanVertexBuffer::Initialize(VulkanDevice* device, const void* vertices, VkDeviceSize size, VkCommandPool commandPool)
{
    m_device = device;
    m_size = size;
    
    return CreateVertexBuffer(device, vertices, size, commandPool);
}

bool VulkanVertexBuffer::Initialize(VulkanDevice* device, VkDeviceSize size)
{
    m_device = device;
    m_size = size;
    
    // Create vertex buffer with HOST_VISIBLE memory for dynamic updates
    m_buffer = new VulkanBuffer();
    if (!m_buffer->Initialize(device, size,
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        std::cerr << "Failed to create vertex buffer!" << '\n';
        delete m_buffer;
        m_buffer = nullptr;
        return false;
    }
    
    return true;
}

void VulkanVertexBuffer::Cleanup()
{
    if (m_buffer)
    {
        m_buffer->Cleanup();
        delete m_buffer;
        m_buffer = nullptr;
    }
    
    m_device = nullptr;
    m_size = 0;
    m_vertexCount = 0;
}

void VulkanVertexBuffer::UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (!m_buffer)
    {
        std::cerr << "Cannot update data: vertex buffer not initialized!" << std::endl;
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

void VulkanVertexBuffer::Bind(VkCommandBuffer commandBuffer, uint32_t binding)
{
    if (!m_buffer)
    {
        std::cerr << "Cannot bind: vertex buffer not initialized!" << std::endl;
        return;
    }
    
    VkBuffer buffer = m_buffer->GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, binding, 1, &buffer, &offset);
}

bool VulkanVertexBuffer::CreateVertexBuffer(VulkanDevice* device, const void* vertices, VkDeviceSize size, VkCommandPool commandPool)
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
    
    // Copy vertex data to staging buffer
    stagingBuffer.CopyData(vertices, size);
    
    // Create vertex buffer on device local memory
    m_buffer = new VulkanBuffer();
    if (!m_buffer->Initialize(device, size,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
        std::cerr << "Failed to create vertex buffer!" << std::endl;
        stagingBuffer.Cleanup();
        delete m_buffer;
        m_buffer = nullptr;
        return false;
    }
    
    // Copy from staging buffer to vertex buffer
    VkCommandBuffer commandBuffer = device->BeginSingleTimeCommands(commandPool);
    m_buffer->CopyFrom(commandBuffer, &stagingBuffer, size);
    device->EndSingleTimeCommands(commandPool, device->GetGraphicsQueue(), commandBuffer);
    
    // Cleanup staging buffer
    stagingBuffer.Cleanup();
    return true;
}

#endif // RENDER_API_VULKAN