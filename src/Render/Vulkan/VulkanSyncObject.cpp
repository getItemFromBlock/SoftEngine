#include "VulkanSyncObjects.h"
#ifdef RENDER_API_VULKAN

#include "VulkanDevice.h"
#include <iostream>
#include <stdexcept>
#include <string>

VulkanSyncObjects::~VulkanSyncObjects() = default;

bool VulkanSyncObjects::Initialize(VulkanDevice* device, uint32_t maxFramesInFlight)
{
    if (!device || maxFramesInFlight == 0)
    {
        std::cerr << "Invalid parameters for sync objects initialization!" << std::endl;
        return false;
    }

    m_device = device;

    try
    {
        m_imageAvailableSemaphores.resize(maxFramesInFlight);
        m_renderFinishedSemaphores.resize(maxFramesInFlight);
        m_inFlightFences.resize(maxFramesInFlight);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled

        for (size_t i = 0; i < maxFramesInFlight; i++)
        {
            VkResult result = vkCreateSemaphore(m_device->GetDevice(), &semaphoreInfo, 
                                               nullptr, &m_imageAvailableSemaphores[i]);
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image available semaphore " + 
                                         std::to_string(i) + "! Error code: " + std::to_string(result));
            }

            result = vkCreateSemaphore(m_device->GetDevice(), &semaphoreInfo, 
                                      nullptr, &m_renderFinishedSemaphores[i]);
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create render finished semaphore " + 
                                         std::to_string(i) + "! Error code: " + std::to_string(result));
            }

            result = vkCreateFence(m_device->GetDevice(), &fenceInfo, 
                                  nullptr, &m_inFlightFences[i]);
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create fence " + std::to_string(i) + 
                                         "! Error code: " + std::to_string(result));
            }
        }

        std::cout << "Created synchronization objects for " << maxFramesInFlight << " frames" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Sync objects initialization failed: " << e.what() << std::endl;
        Cleanup();
        return false;
    }
}

void VulkanSyncObjects::Cleanup()
{
    if (m_device)
    {
        for (size_t i = 0; i < m_inFlightFences.size(); i++)
        {
            if (m_imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_device->GetDevice(), m_imageAvailableSemaphores[i], nullptr);
            }
            if (m_renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(m_device->GetDevice(), m_renderFinishedSemaphores[i], nullptr);
            }
            if (m_inFlightFences[i] != VK_NULL_HANDLE)
            {
                vkDestroyFence(m_device->GetDevice(), m_inFlightFences[i], nullptr);
            }
        }
    }

    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();
}

void VulkanSyncObjects::WaitForFence(size_t index)
{
    vkWaitForFences(m_device->GetDevice(), 1, &m_inFlightFences[index], VK_TRUE, UINT64_MAX);
}

void VulkanSyncObjects::ResetFence(size_t index)
{
    vkResetFences(m_device->GetDevice(), 1, &m_inFlightFences[index]);
}

#endif