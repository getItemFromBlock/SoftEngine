#include "VulkanBuffer.h"
#ifdef RENDER_API_VULKAN

#include <cassert>

#include "VulkanDevice.h"
#include <stdexcept>
#include <cstring>
#include <iostream>

VulkanBuffer::~VulkanBuffer()
{
    Cleanup();
}

bool VulkanBuffer::Initialize(VulkanDevice* device, VkDeviceSize size,
                              VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    m_device = device;
    m_size = size;

    // Create buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device->GetDevice(), &bufferInfo, nullptr, &m_buffer) != VK_SUCCESS)
    {
        std::cerr << "Failed to create buffer!" << std::endl;
        return false;
    }

    // Get memory requirements
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device->GetDevice(), m_buffer, &memRequirements);

    // Allocate memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);
    
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(
        m_device->GetPhysicalDevice(), &memProperties);

    if (vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &m_bufferMemory) != VK_SUCCESS)
    {
        return false;
    }

    // Bind buffer memory
    vkBindBufferMemory(m_device->GetDevice(), m_buffer, m_bufferMemory, 0);

    return true;
}

void VulkanBuffer::Cleanup()
{
    if (m_device)
    {
        if (m_buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(m_device->GetDevice(), m_buffer, nullptr);
            m_buffer = VK_NULL_HANDLE;
        }

        if (m_bufferMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_device->GetDevice(), m_bufferMemory, nullptr);
            m_bufferMemory = VK_NULL_HANDLE;
        }
    }
}

void VulkanBuffer::CopyData(const void* data, VkDeviceSize size)
{
    void* mappedData;
    vkMapMemory(m_device->GetDevice(), m_bufferMemory, 0, size, 0, &mappedData);
    memcpy(mappedData, data, static_cast<size_t>(size));
    vkUnmapMemory(m_device->GetDevice(), m_bufferMemory);
}

void VulkanBuffer::CopyFrom(VkCommandBuffer commandBuffer, VulkanBuffer* srcBuffer, VkDeviceSize size)
{
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer->GetBuffer(), m_buffer, 1, &copyRegion);
}

uint32_t VulkanBuffer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_device->GetPhysicalDevice(), &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

#endif