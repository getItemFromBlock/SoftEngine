#include "VulkanSyncObjects.h"
#ifdef RENDER_API_VULKAN

#include "VulkanDevice.h"
#include <iostream>
#include <stdexcept>
#include <string>

#include "Debug/Log.h"

VulkanSyncObjects::~VulkanSyncObjects()
{
    Cleanup();
}


bool VulkanSyncObjects::Initialize(VulkanDevice* device, uint32_t maxFramesInFlight)
{
    if (!device || maxFramesInFlight == 0) return false;
    m_device = device;

    try
    {
        m_imageAvailableSemaphores.resize(maxFramesInFlight);
        m_inFlightFences.resize(maxFramesInFlight);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < maxFramesInFlight; i++)
        {
            if (vkCreateSemaphore(m_device->GetDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create semaphore");
                
            if (vkCreateFence(m_device->GetDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create fence");
        }
        return true;
    }
    catch (const std::exception& e)
    {
        PrintError("Sync objects init failed: %s", e.what());
        Cleanup();
        return false;
    }
}

void VulkanSyncObjects::Cleanup()
{
    if (m_device)
    {
        for (auto semaphore : m_imageAvailableSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(m_device->GetDevice(), semaphore, nullptr);
        }
        for (auto semaphore : m_renderFinishedSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(m_device->GetDevice(), semaphore, nullptr);
        }
        for (auto fence : m_inFlightFences)
        {
            if (fence != VK_NULL_HANDLE) vkDestroyFence(m_device->GetDevice(), fence, nullptr);
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

bool VulkanSyncObjects::ResizeRenderFinishedSemaphores(uint32_t swapChainImageCount)
{
    m_renderFinishedSemaphores.resize(swapChainImageCount);
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < swapChainImageCount; i++)
    {
        if (vkCreateSemaphore(m_device->GetDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS)
        {
            PrintError("Failed to recreate render finished semaphores");
            return false;
        }
    }
    return true;
}

#endif
