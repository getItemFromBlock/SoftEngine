#include "VulkanGBuffer.h"
#include "VulkanDevice.h"
#include "Debug/Log.h"
#include <stdexcept>

VulkanGBuffer::~VulkanGBuffer()
{
    Cleanup();
}

bool VulkanGBuffer::Initialize(VulkanDevice* device, uint32_t width, uint32_t height)
{
    if (!device)
    {
        PrintError("VulkanGBuffer: invalid device");
        return false;
    }

    m_device = device;
    m_width = width;
    m_height = height;
    m_firstUse = true;

    if (!CreateAttachment(m_position, kPositionFormat)) 
        return false;
    if (!CreateAttachment(m_normal, kNormalFormat)) 
        return false;
    if (!CreateAttachment(m_albedo, kAlbedoFormat)) 
        return false;
    if (!CreateAttachment(m_metallicRoughness, kMetallicRoughnessFormat)) 
        return false;
    if (!CreateSampler()) 
        return false;

    return true;
}

bool VulkanGBuffer::CreateAttachment(GBufferAttachment& attachment, VkFormat format) const
{
    attachment.format = format;

    constexpr VkImageUsageFlags kUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = format;
    imageInfo.extent = {m_width, m_height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = kUsage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(m_device->GetDevice(), &imageInfo, nullptr, &attachment.image) != VK_SUCCESS)
    {
        PrintError("VulkanGBuffer: failed to create image");
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(m_device->GetDevice(), attachment.image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &attachment.memory) != VK_SUCCESS)
    {
        PrintError("VulkanGBuffer: failed to allocate memory");
        return false;
    }

    vkBindImageMemory(m_device->GetDevice(), attachment.image, attachment.memory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = attachment.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device->GetDevice(), &viewInfo, nullptr, &attachment.imageView) != VK_SUCCESS)
    {
        PrintError("VulkanGBuffer: failed to create image view");
        return false;
    }

    return true;
}

bool VulkanGBuffer::CreateSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

    if (vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
    {
        PrintError("VulkanGBuffer: failed to create sampler");
        return false;
    }

    return true;
}

void VulkanGBuffer::DestroyAttachment(GBufferAttachment& attachment)
{
    VkDevice dev = m_device->GetDevice();
    if (attachment.imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(dev, attachment.imageView, nullptr);
        attachment.imageView = VK_NULL_HANDLE;
    }
    if (attachment.image != VK_NULL_HANDLE)
    {
        vkDestroyImage(dev, attachment.image, nullptr);
        attachment.image = VK_NULL_HANDLE;
    }
    if (attachment.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(dev, attachment.memory, nullptr);
        attachment.memory = VK_NULL_HANDLE;
    }
}

void VulkanGBuffer::Cleanup()
{
    if (!m_device) return;

    if (m_sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device->GetDevice(), m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    DestroyAttachment(m_position);
    DestroyAttachment(m_normal);
    DestroyAttachment(m_albedo);
    DestroyAttachment(m_metallicRoughness);
}

bool VulkanGBuffer::Resize(uint32_t width, uint32_t height)
{
    Cleanup();
    return Initialize(m_device, width, height);
}
