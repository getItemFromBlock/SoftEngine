#include "VulkanDepthBuffer.h"
#include "VulkanDevice.h"

#include <iostream>
#include <stdexcept>
#include <algorithm>

#include "VulkanCommandPool.h"
#include "Debug/Log.h"

VulkanDepthBuffer::~VulkanDepthBuffer()
{
    Cleanup();
}

bool VulkanDepthBuffer::Initialize(VulkanDevice* device, VkExtent2D extent)
{
    if (!device)
    {
        std::cerr << "Invalid device pointer!" << std::endl;
        return false;
    }

    m_device = device;
    m_depthFormat = FindDepthFormat(device);

    try
    {
        // Create depth image
        CreateImage(device, extent.width, extent.height, m_depthFormat,
                   VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_depthImage, m_depthImageMemory);

        // Create depth image view
        m_depthImageView = CreateImageView(device, m_depthImage, m_depthFormat,
                                          VK_IMAGE_ASPECT_DEPTH_BIT);

        return true;
    }
    catch (const std::exception& e)
    {
        PrintError("Failed to initialize depth buffer: %s", e.what());
        Cleanup();
        return false;
    }
}

void VulkanDepthBuffer::Cleanup()
{
    if (m_device && m_device->GetDevice())
    {
        if (m_depthImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(m_device->GetDevice(), m_depthImageView, nullptr);
            m_depthImageView = VK_NULL_HANDLE;
        }

        if (m_depthImage != VK_NULL_HANDLE)
        {
            vkDestroyImage(m_device->GetDevice(), m_depthImage, nullptr);
            m_depthImage = VK_NULL_HANDLE;
        }

        if (m_depthImageMemory != VK_NULL_HANDLE)
        {
            vkFreeMemory(m_device->GetDevice(), m_depthImageMemory, nullptr);
            m_depthImageMemory = VK_NULL_HANDLE;
        }
    }

    m_depthFormat = VK_FORMAT_UNDEFINED;
    m_device = nullptr;
}

VkFormat VulkanDepthBuffer::FindDepthFormat(VulkanDevice* device)
{
    return FindSupportedFormat(
        device,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

VkFormat VulkanDepthBuffer::FindSupportedFormat(VulkanDevice* device,
                                                const std::vector<VkFormat>& candidates,
                                                VkImageTiling tiling,
                                                VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(device->GetPhysicalDevice(), format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format!");
}

bool VulkanDepthBuffer::HasStencilComponent(VkFormat format)
{
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void VulkanDepthBuffer::CreateImage(VulkanDevice* device, uint32_t width, uint32_t height,
                                   VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage,
                                   VkMemoryPropertyFlags properties, VkImage& image,
                                   VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device->GetDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image!");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device->GetDevice(), image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = device->FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device->GetDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate image memory!");
    }

    vkBindImageMemory(device->GetDevice(), image, imageMemory, 0);
}

VkImageView VulkanDepthBuffer::CreateImageView(VulkanDevice* device, VkImage image,
                                               VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    if (vkCreateImageView(device->GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image view!");
    }

    return imageView;
}