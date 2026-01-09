#include "VulkanVertexBuffer.h"

#include "VulkanDevice.h"
#include <iostream>

#include "VulkanCommandPool.h"
#include "Debug/Log.h"

VulkanVertexBuffer::~VulkanVertexBuffer()
{
    Cleanup();
}

bool VulkanVertexBuffer::Initialize(VulkanDevice* device, const void* vertices, VkDeviceSize size, VulkanCommandPool* commandBuffer)
{
    m_device = device;
    m_size = size;
    
    return CreateVertexBuffer(device, vertices, size, commandBuffer);
}

bool VulkanVertexBuffer::Initialize(VulkanDevice* device, VkDeviceSize size)
{
    m_device = device;
    m_size = size;
    
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
        PrintError("Cannot update data: vertex buffer not initialized!");
        return;
    }
    
    if (offset + size > m_size)
    {
        PrintError("Update data exceeds buffer size!");
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
        PrintError("Cannot bind: vertex buffer not initialized!");
        return;
    }
    
    VkBuffer buffer = m_buffer->GetBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffer, binding, 1, &buffer, &offset);
}

bool VulkanVertexBuffer::CreateVertexBuffer(VulkanDevice* device, const void* vertices, VkDeviceSize size, VulkanCommandPool* commandPool)
{
    VulkanBuffer stagingBuffer;
    if (!stagingBuffer.Initialize(device, size,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        PrintError("Failed to create staging buffer!");
        return false;
    }
    
    stagingBuffer.CopyData(vertices, size);
    
    m_buffer = new VulkanBuffer();
    if (!m_buffer->Initialize(device, size,
                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
))
    {
        PrintError("Failed to create vertex buffer!");
        stagingBuffer.Cleanup();
        delete m_buffer;
        m_buffer = nullptr;
        return false;
    }
    
    VkCommandBuffer newCommandBuffer = device->BeginSingleTimeCommands(commandPool);
    m_buffer->CopyFrom(newCommandBuffer, &stagingBuffer, size);
    device->EndSingleTimeCommands(commandPool, device->GetGraphicsQueue(), newCommandBuffer);
    
    stagingBuffer.Cleanup();
    return true;
}
