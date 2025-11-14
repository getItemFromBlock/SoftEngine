#include "VulkanSwapChain.h"
#ifdef RENDER_API_VULKAN

#include "VulkanDevice.h"
#include "Core/Window.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

bool VulkanSwapChain::Initialize(VulkanDevice* device, VkSurfaceKHR surface, Window* window)
{
    if (!device || surface == VK_NULL_HANDLE || !window)
    {
        std::cerr << "Invalid parameters for swap chain initialization!" << std::endl;
        return false;
    }

    m_device = device;
    m_surface = surface;

    try
    {
        CreateSwapChain(window);
        CreateImageViews();
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Swap chain initialization failed: " << e.what() << std::endl;
        Cleanup();
        return false;
    }
}

void VulkanSwapChain::Cleanup()
{
    CleanupSwapChain();
}

void VulkanSwapChain::Recreate(Window* window)
{
    if (!m_device || !window)
    {
        return;
    }

    // Wait for device to be idle before recreating
    vkDeviceWaitIdle(m_device->GetDevice());

    CleanupSwapChain();

    try
    {
        CreateSwapChain(window);
        CreateImageViews();
        std::cout << "Swap chain recreated successfully" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Swap chain recreation failed: " << e.what() << std::endl;
        throw;
    }
}

VkResult VulkanSwapChain::AcquireNextImage(VkSemaphore semaphore, uint32_t* imageIndex)
{
    return vkAcquireNextImageKHR(
        m_device->GetDevice(),
        m_swapChain,
        UINT64_MAX,
        semaphore,
        VK_NULL_HANDLE,
        imageIndex
    );
}

VkResult VulkanSwapChain::PresentImage(VkQueue presentQueue, uint32_t imageIndex, VkSemaphore waitSemaphore)
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapChain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = nullptr;

    return vkQueuePresentKHR(presentQueue, &presentInfo);
}

VulkanSwapChain::SwapChainSupportDetails VulkanSwapChain::QuerySwapChainSupport(
    VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR VulkanSwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    // Prefer SRGB if available
    for (const auto& format : availableFormats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    // Fall back to first available format
    return availableFormats[0];
}

VkPresentModeKHR VulkanSwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    // Prefer mailbox (triple buffering) for smooth rendering
    for (const auto& mode : availablePresentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }

    // FIFO is guaranteed to be available
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapChain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    Vec2i windowSize = window->GetSize();

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(windowSize.x),
        static_cast<uint32_t>(windowSize.y)
    };

    actualExtent.width = std::clamp(actualExtent.width,
                                     capabilities.minImageExtent.width,
                                     capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
                                      capabilities.minImageExtent.height,
                                      capabilities.maxImageExtent.height);

    return actualExtent;
}

void VulkanSwapChain::CreateSwapChain(Window* window)
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(
        m_device->GetPhysicalDevice(), m_surface);

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities, window);

    // Request one more than minimum to avoid waiting
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    auto indices = m_device->GetQueueFamilyIndices();
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(m_device->GetDevice(), &createInfo, nullptr, &m_swapChain);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create swap chain! Error code: " + std::to_string(result));
    }

    // Retrieve swap chain images
    vkGetSwapchainImagesKHR(m_device->GetDevice(), m_swapChain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device->GetDevice(), m_swapChain, &imageCount, m_images.data());

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;
}

void VulkanSwapChain::CreateImageViews()
{
    m_imageViews.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(m_device->GetDevice(), &createInfo, nullptr, &m_imageViews[i]);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create image view " + std::to_string(i) + 
                                     "! Error code: " + std::to_string(result));
        }
    }
}

void VulkanSwapChain::CleanupSwapChain()
{
    for (auto imageView : m_imageViews)
    {
        if (imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_device->GetDevice(), imageView, nullptr);
        }
    }
    m_imageViews.clear();

    if (m_swapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device->GetDevice(), m_swapChain, nullptr);
        m_swapChain = VK_NULL_HANDLE;
    }

    m_images.clear();
}

#endif