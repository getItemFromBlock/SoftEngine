#include "VulkanCommandBuffer.h"
#ifdef RENDER_API_VULKAN

#include "VulkanDevice.h"
#include <iostream>
#include <stdexcept>
#include <string>

bool VulkanCommandBuffer::Initialize(VulkanDevice* device, uint32_t imageCount)
{
    if (!device || imageCount == 0)
    {
        std::cerr << "Invalid parameters for command buffer initialization!" << std::endl;
        return false;
    }

    m_device = device;

    try
    {
        // Create command pool
        auto queueFamilyIndices = m_device->GetQueueFamilyIndices();

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        VkResult result = vkCreateCommandPool(m_device->GetDevice(), &poolInfo, 
                                             nullptr, &m_commandPool);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool! Error code: " + 
                                     std::to_string(result));
        }

        // Allocate command buffers
        m_commandBuffers.resize(imageCount);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = imageCount;

        result = vkAllocateCommandBuffers(m_device->GetDevice(), &allocInfo, m_commandBuffers.data());
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffers! Error code: " + 
                                     std::to_string(result));
        }

        std::cout << "Created command pool and allocated " << imageCount << " command buffers" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Command buffer initialization failed: " << e.what() << std::endl;
        Cleanup();
        return false;
    }
}

void VulkanCommandBuffer::Cleanup()
{
    if (m_device && m_commandPool != VK_NULL_HANDLE)
    {
        if (!m_commandBuffers.empty())
        {
            vkFreeCommandBuffers(m_device->GetDevice(), m_commandPool,
                               static_cast<uint32_t>(m_commandBuffers.size()),
                               m_commandBuffers.data());
            m_commandBuffers.clear();
        }

        vkDestroyCommandPool(m_device->GetDevice(), m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
}

void VulkanCommandBuffer::BeginRecording(size_t index)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    VkResult result = vkBeginCommandBuffer(m_commandBuffers[index], &beginInfo);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin recording command buffer! Error code: " + 
                                 std::to_string(result));
    }
}

void VulkanCommandBuffer::EndRecording(size_t index)
{
    VkResult result = vkEndCommandBuffer(m_commandBuffers[index]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to end recording command buffer! Error code: " + 
                                 std::to_string(result));
    }
}

void VulkanCommandBuffer::Reset(size_t index)
{
    vkResetCommandBuffer(m_commandBuffers[index], 0);
}

#endif