#include "VulkanDevice.h"
#ifdef RENDER_API_VULKAN

#include <iostream>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <string>

#include "VulkanCommandPool.h"
#include "Debug/Log.h"

VulkanDevice::~VulkanDevice()
{
    Cleanup();
}

bool VulkanDevice::Initialize(VkInstance instance, VkSurfaceKHR surface)
{
    if (instance == VK_NULL_HANDLE || surface == VK_NULL_HANDLE)
    {
        PrintError("Invalid instance or surface handle");
        return false;
    }

    try
    {
        if (!PickPhysicalDevice(instance, surface))
        {
            return false;
        }
        CreateLogicalDevice(surface);
        return true;
    }
    catch (const std::exception& e)
    {
        PrintError("VulkanDevice initialization failed: %s", e.what());
        return false;
    }
}

void VulkanDevice::Cleanup()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    // Reset other handles
    m_physicalDevice = VK_NULL_HANDLE;
    m_graphicsQueue.handle = VK_NULL_HANDLE;
    m_presentQueue.handle = VK_NULL_HANDLE;
    m_queueFamilies = QueueFamilyIndices{};
}

uint32_t VulkanDevice::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

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

VkCommandBuffer VulkanDevice::BeginSingleTimeCommands(VulkanCommandPool* commandPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool->GetTransferCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer newCommandBuffer;
    vkAllocateCommandBuffers(m_device, &allocInfo, &newCommandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(newCommandBuffer, &beginInfo);

    return newCommandBuffer;
}

void VulkanDevice::EndSingleTimeCommands(VulkanCommandPool* commandPool, VulkanQueue& queue,
                                         VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    {
        std::scoped_lock lock(*queue.mutex);
        vkQueueSubmit(queue.handle, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue.handle);
    }
    
    {
        vkFreeCommandBuffers(m_device, commandPool->GetTransferCommandPool(), 1, &commandBuffer);
    }
}

bool VulkanDevice::PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface)
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        PrintError("Failed to find suitable GPU for Vulkan");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Try to find the best suitable device
    VkPhysicalDevice selectedDevice = VK_NULL_HANDLE;
    int bestScore = -1;

    for (const auto& device : devices)
    {
        if (IsDeviceSuitable(device, surface))
        {
            int score = RateDeviceSuitability(device);
            if (score > bestScore)
            {
                bestScore = score;
                selectedDevice = device;
            }
        }
    }

    if (selectedDevice == VK_NULL_HANDLE)
    {
        PrintError("Failed to find a suitable GPU!");
        return false;
    }

    m_physicalDevice = selectedDevice;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
    PrintLog("Using GPU: %s", deviceProperties.deviceName);

    m_queueFamilies = FindQueueFamilies(m_physicalDevice, surface);

    return m_queueFamilies.isComplete();
}

int VulkanDevice::RateDeviceSuitability(VkPhysicalDevice device) const
{
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    int score = 0;
    
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
        score += 1000;
    }
    else if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
    {
        score += 100;
    }

    score += deviceProperties.limits.maxImageDimension2D;

    return score;
}

bool VulkanDevice::IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const
{
    QueueFamilyIndices indices = FindQueueFamilies(device, surface);
    bool extensionsSupported = CheckDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        // Check if swap chain support is adequate
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

        swapChainAdequate = formatCount > 0 && presentModeCount > 0;
    }

    // Check if dynamic rendering is supported
    bool dynamicRenderingSupported = CheckDynamicRenderingSupport(device);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && dynamicRenderingSupported;
}

bool VulkanDevice::CheckDynamicRenderingSupport(VkPhysicalDevice device) const
{
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;

    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &dynamicRenderingFeatures;

    vkGetPhysicalDeviceFeatures2(device, &deviceFeatures2);

    if (!dynamicRenderingFeatures.dynamicRendering)
    {
        PrintLog("Warning: Dynamic rendering is not supported on this device");
        return false;
    }

    return true;
}

bool VulkanDevice::CheckDeviceExtensionSupport(VkPhysicalDevice device) const
{
    const std::vector<const char*> requiredExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME  // Add dynamic rendering extension
    };

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    for (const char* required : requiredExtensions)
    {
        bool found = false;
        for (const auto& available : availableExtensions)
        {
            if (strcmp(required, available.extensionName) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            PrintLog("Required extension not found: %s", required);
            return false;
        }
    }

    return true;
}

VulkanDevice::QueueFamilyIndices VulkanDevice::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const
{
    QueueFamilyIndices indices;

    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueCount; ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }
    }

    return indices;
}

void VulkanDevice::CreateLogicalDevice(VkSurfaceKHR surface)
{
    QueueFamilyIndices indices = FindQueueFamilies(m_physicalDevice, surface);

    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());

    float queuePriority = 1.0f;

    for (uint32_t family : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = family;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Enable dynamic rendering feature
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures{};
    dynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;
    dynamicRenderingFeatures.pNext = nullptr;

    // Device features
    VkPhysicalDeviceFeatures2 deviceFeatures2{};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &dynamicRenderingFeatures;  // Chain dynamic rendering features
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

    // Required extensions
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &deviceFeatures2;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create logical device! Error code: " + std::to_string(result));
    }

    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue.handle);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue.handle);
        
    if (indices.graphicsFamily == indices.presentFamily)
    {
        m_presentQueue.mutex = m_graphicsQueue.mutex;
    }
}

#endif