#include "VulkanUniformBuffer.h"
#ifdef RENDER_API_VULKAN

#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include <iostream>
#include <cstring>

VulkanUniformBuffer::~VulkanUniformBuffer()
{
    Cleanup();
}

bool VulkanUniformBuffer::Initialize(VulkanDevice* device, VkDeviceSize size, 
                                     uint32_t frameCount, VkBufferUsageFlags usageFlags)
{
    if (!device || size == 0 || frameCount == 0)
    {
        std::cerr << "VulkanUniformBuffer::Initialize - invalid parameters\n";
        return false;
    }

    Cleanup();

    m_device = device;
    m_size = size;
    m_buffer.resize(frameCount, nullptr);
    m_mappedMemory.resize(frameCount, nullptr);

    for (uint32_t i = 0; i < frameCount; ++i)
    {
        VulkanBuffer* buf = new VulkanBuffer();
        if (!buf->Initialize(device, size,
                             usageFlags,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            std::cerr << "VulkanUniformBuffer::Initialize - failed to create uniform buffer for frame " << i << "\n";
            delete buf;
            buf = nullptr;
            Cleanup();
            return false;
        }

        m_buffer[i] = buf;
    }

    return true;
}

void VulkanUniformBuffer::Cleanup()
{
    // Unmap any mapped memories
    UnmapAll();

    for (auto b : m_buffer)
    {
        if (b)
        {
            b->Cleanup();
            delete b;
        }
    }

    m_buffer.clear();
    m_mappedMemory.clear();
    m_device = nullptr;
    m_size = 0;
}

void VulkanUniformBuffer::UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (m_buffer.empty())
    {
        std::cerr << "VulkanUniformBuffer::UpdateData - not initialized\n";
        return;
    }
    if (!data)
    {
        std::cerr << "VulkanUniformBuffer::UpdateData - data is null\n";
        return;
    }
    if (offset + size > m_size)
    {
        std::cerr << "VulkanUniformBuffer::UpdateData - write exceeds buffer size\n";
        return;
    }

    // Write to every frame buffer
    for (size_t i = 0; i < m_buffer.size(); ++i)
    {
        // If mapped, write directly
        if (m_mappedMemory[i])
        {
            std::memcpy(static_cast<uint8_t*>(m_mappedMemory[i]) + offset, data, static_cast<size_t>(size));
            continue;
        }

        // Map temporarily
        void* mapped = nullptr;
        VkResult res = vkMapMemory(m_device->GetDevice(), m_buffer[i]->GetBufferMemory(), offset, size, 0, &mapped);
        if (res != VK_SUCCESS)
        {
            std::cerr << "VulkanUniformBuffer::UpdateData - vkMapMemory failed for frame " << i << "\n";
            continue;
        }
        std::memcpy(mapped, data, static_cast<size_t>(size));
        vkUnmapMemory(m_device->GetDevice(), m_buffer[i]->GetBufferMemory());
    }
}

void VulkanUniformBuffer::UpdateData(const void* data, VkDeviceSize size, uint32_t currentFrame, VkDeviceSize offset)
{
    if (m_buffer.empty())
    {
        std::cerr << "VulkanUniformBuffer::UpdateData(frame) - not initialized\n";
        return;
    }
    if (currentFrame >= m_buffer.size())
    {
        std::cerr << "VulkanUniformBuffer::UpdateData(frame) - invalid frame index " << currentFrame << "\n";
        return;
    }
    if (!data)
    {
        std::cerr << "VulkanUniformBuffer::UpdateData(frame) - data is null\n";
        return;
    }
    if (offset + size > m_size)
    {
        std::cerr << "VulkanUniformBuffer::UpdateData(frame) - write exceeds buffer size\n";
        return;
    }

    // If mapped, write directly
    if (m_mappedMemory[currentFrame])
    {
        std::memcpy(static_cast<uint8_t*>(m_mappedMemory[currentFrame]) + offset, data, static_cast<size_t>(size));
        return;
    }

    // Map temporarily
    void* mapped = nullptr;
    VkResult res = vkMapMemory(m_device->GetDevice(), m_buffer[currentFrame]->GetBufferMemory(), offset, size, 0, &mapped);
    if (res != VK_SUCCESS)
    {
        std::cerr << "VulkanUniformBuffer::UpdateData(frame) - vkMapMemory failed for frame " << currentFrame << "\n";
        return;
    }
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(m_device->GetDevice(), m_buffer[currentFrame]->GetBufferMemory());
}

void* VulkanUniformBuffer::Map(uint32_t currentFrame)
{
    if (m_buffer.empty())
    {
        std::cerr << "VulkanUniformBuffer::Map - not initialized\n";
        return nullptr;
    }
    if (currentFrame >= m_buffer.size())
    {
        std::cerr << "VulkanUniformBuffer::Map - invalid frame index " << currentFrame << "\n";
        return nullptr;
    }

    if (m_mappedMemory[currentFrame])
    {
        return m_mappedMemory[currentFrame]; // already mapped
    }

    void* mapped = nullptr;
    VkResult res = vkMapMemory(m_device->GetDevice(), m_buffer[currentFrame]->GetBufferMemory(), 0, m_size, 0, &mapped);
    if (res != VK_SUCCESS)
    {
        std::cerr << "VulkanUniformBuffer::Map - vkMapMemory failed for frame " << currentFrame << "\n";
        return nullptr;
    }

    m_mappedMemory[currentFrame] = mapped;
    return mapped;
}

bool VulkanUniformBuffer::MapAll()
{
    if (m_buffer.empty())
    {
        std::cerr << "VulkanUniformBuffer::MapAll - not initialized\n";
        return false;
    }

    for (size_t i = 0; i < m_buffer.size(); ++i)
    {
        if (m_mappedMemory[i]) continue; // already mapped

        void* mapped = nullptr;
        VkResult res = vkMapMemory(m_device->GetDevice(), m_buffer[i]->GetBufferMemory(), 0, m_size, 0, &mapped);
        if (res != VK_SUCCESS)
        {
            std::cerr << "VulkanUniformBuffer::MapAll - vkMapMemory failed for frame " << i << "\n";
            for (size_t j = 0; j < i; ++j)
            {
                if (m_mappedMemory[j])
                {
                    vkUnmapMemory(m_device->GetDevice(), m_buffer[j]->GetBufferMemory());
                    m_mappedMemory[j] = nullptr;
                }
            }
            return false;
        }
        m_mappedMemory[i] = mapped;
    }
    return true;
}

void VulkanUniformBuffer::Unmap(uint32_t currentFrame)
{
    if (m_buffer.empty() || currentFrame >= m_buffer.size()) return;

    if (m_mappedMemory[currentFrame])
    {
        vkUnmapMemory(m_device->GetDevice(), m_buffer[currentFrame]->GetBufferMemory());
        m_mappedMemory[currentFrame] = nullptr;
    }
}

void VulkanUniformBuffer::UnmapAll()
{
    if (m_buffer.empty()) return;

    for (size_t i = 0; i < m_buffer.size(); ++i)
    {
        if (m_mappedMemory[i])
        {
            vkUnmapMemory(m_device->GetDevice(), m_buffer[i]->GetBufferMemory());
            m_mappedMemory[i] = nullptr;
        }
    }
}

void VulkanUniformBuffer::WriteToMapped(const void* data, VkDeviceSize size, uint32_t currentFrame, VkDeviceSize offset)
{
    if (m_buffer.empty())
    {
        std::cerr << "VulkanUniformBuffer::WriteToMapped - not initialized\n";
        return;
    }
    if (currentFrame >= m_buffer.size())
    {
        std::cerr << "VulkanUniformBuffer::WriteToMapped - invalid frame index\n";
        return;
    }
    if (!m_mappedMemory[currentFrame])
    {
        std::cerr << "VulkanUniformBuffer::WriteToMapped - frame not mapped (call Map(frame) first)\n";
        return;
    }
    if (offset + size > m_size)
    {
        std::cerr << "VulkanUniformBuffer::WriteToMapped - write exceeds buffer size\n";
        return;
    }

    std::memcpy(static_cast<uint8_t*>(m_mappedMemory[currentFrame]) + offset, data, static_cast<size_t>(size));
}

VkBuffer VulkanUniformBuffer::GetBuffer(size_t i) const
{
    return (i < m_buffer.size() && m_buffer[i]) ? m_buffer[i]->GetBuffer() : VK_NULL_HANDLE;
}

VkDescriptorBufferInfo VulkanUniformBuffer::GetDescriptorInfo(size_t frame) const
{
    VkDescriptorBufferInfo bufferInfo{};
    if (frame < m_buffer.size() && m_buffer[frame])
    {
        bufferInfo.buffer = m_buffer[frame]->GetBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = m_size;
    }
    else
    {
        bufferInfo.buffer = VK_NULL_HANDLE;
        bufferInfo.offset = 0;
        bufferInfo.range = 0;
    }
    return bufferInfo;
}

VkDescriptorBufferInfo VulkanUniformBuffer::GetDescriptorInfo(size_t frame, VkDeviceSize offset, VkDeviceSize range) const
{
    VkDescriptorBufferInfo bufferInfo{};
    if (frame < m_buffer.size() && m_buffer[frame])
    {
        bufferInfo.buffer = m_buffer[frame]->GetBuffer();
        bufferInfo.offset = offset;
        bufferInfo.range = range;
    }
    else
    {
        bufferInfo.buffer = VK_NULL_HANDLE;
        bufferInfo.offset = 0;
        bufferInfo.range = 0;
    }
    return bufferInfo;
}

#endif // RENDER_API_VULKAN
