#ifdef RENDER_API_VULKAN

#include "VulkanTexture.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include <stb_image.h>
#include <stdexcept>
#include <cstring>

VulkanTexture::~VulkanTexture()
{
    Cleanup();
}

bool VulkanTexture::LoadFromFile(VulkanDevice* device, const std::string& filepath,
                                 VkCommandPool commandPool, VkQueue graphicsQueue)
{
    if (!device) {
        return false;
    }
    
    m_device = device;
    
    // Load image using stb_image
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    
    if (!pixels) {
        return false;
    }
    
    p_width = static_cast<uint32_t>(texWidth);
    p_height = static_cast<uint32_t>(texHeight);
    VkDeviceSize imageSize = p_width * p_height * 4;
    
    // Use RAII wrapper for automatic cleanup
    VulkanBuffer stagingBuffer;
    
    if (!stagingBuffer.Initialize(m_device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        stbi_image_free(pixels);
        return false;
    }
    
    // Copy pixel data to staging buffer
    if (!CopyDataToBuffer(stagingBuffer, pixels, imageSize)) {
        stbi_image_free(pixels);
        return false;
    }
    
    stbi_image_free(pixels);
    
    // Create and setup image
    if (!CreateAndSetupImage(stagingBuffer.GetBuffer(), commandPool, graphicsQueue)) {
        return false;
    }
    
    return true;
}

bool VulkanTexture::LoadFromImage(const ImageLoader::Image& image, VulkanDevice* device, 
                                  VkCommandPool commandPool, VkQueue graphicsQueue)
{
    if (!device) {
        return false;
    }
    
    m_device = device;
    p_width = image.size.x;
    p_height = image.size.y;
    
    if (!CreateImage(p_width, p_height, m_format, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
        return false;
    }
    
    if (!CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT)) {
        Cleanup();
        return false;
    }
    
    if (!CreateSampler()) {
        Cleanup();
        return false;
    }
    
    return true;
}

bool VulkanTexture::Create(VulkanDevice* device, uint32_t width, uint32_t height,
                           VkFormat format, VkImageUsageFlags usage, 
                           VkCommandPool commandPool, VkQueue graphicsQueue)
{
    if (!device || width == 0 || height == 0) {
        return false;
    }
    
    m_device = device;
    p_width = width;
    p_height = height;
    m_format = format;
    
    if (!CreateImage(width, height, format, VK_IMAGE_TILING_OPTIMAL, usage,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
        return false;
    }
    
    if (!CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT)) {
        Cleanup();
        return false;
    }
    
    if (!CreateSampler()) {
        Cleanup();
        return false;
    }
    
    return true;
}

void VulkanTexture::Cleanup()
{
    if (m_device == nullptr) return;
    
    VkDevice device = m_device->GetDevice();
    
    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }
    
    if (m_imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }
    
    if (m_image != VK_NULL_HANDLE) {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }
    
    if (m_imageMemory != VK_NULL_HANDLE) {
        vkFreeMemory(device, m_imageMemory, nullptr);
        m_imageMemory = VK_NULL_HANDLE;
    }
    
    m_device = nullptr;
}

bool VulkanTexture::CreateImage(uint32_t width, uint32_t height, VkFormat format,
                                VkImageTiling tiling, VkImageUsageFlags usage,
                                VkMemoryPropertyFlags properties)
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
    
    if (vkCreateImage(m_device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device->GetDevice(), m_image, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(memRequirements.memoryTypeBits, properties);
    
    if (vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS) {
        vkDestroyImage(m_device->GetDevice(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return false;
    }
    
    if (vkBindImageMemory(m_device->GetDevice(), m_image, m_imageMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(m_device->GetDevice(), m_imageMemory, nullptr);
        vkDestroyImage(m_device->GetDevice(), m_image, nullptr);
        m_imageMemory = VK_NULL_HANDLE;
        m_image = VK_NULL_HANDLE;
        return false;
    }
    
    return true;
}

bool VulkanTexture::CreateImageView(VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    if (vkCreateImageView(m_device->GetDevice(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS) {
        return false;
    }
    
    return true;
}

bool VulkanTexture::CreateSampler()
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_device->GetPhysicalDevice(), &properties);
    
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    
    if (vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {
        return false;
    }
    
    return true;
}

void VulkanTexture::TransitionImageLayout(VkCommandPool commandPool, VkQueue graphicsQueue,
                                          VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = m_device->BeginSingleTimeCommands(commandPool);
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && 
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else {
        throw std::invalid_argument("Unsupported layout transition!");
    }
    
    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                        0, nullptr, 0, nullptr, 1, &barrier);
    
    m_device->EndSingleTimeCommands(commandPool, graphicsQueue, commandBuffer);
}

void VulkanTexture::CopyBufferToImage(VkCommandPool commandPool, VkQueue graphicsQueue,
                                      VkBuffer buffer, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = m_device->BeginSingleTimeCommands(commandPool);
    
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};
    
    vkCmdCopyBufferToImage(commandBuffer, buffer, m_image, 
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    m_device->EndSingleTimeCommands(commandPool, graphicsQueue, commandBuffer);
}

// Private helper methods
bool VulkanTexture::CopyDataToBuffer(VulkanBuffer& buffer, const void* data, VkDeviceSize size)
{
    void* mappedData;
    if (vkMapMemory(m_device->GetDevice(), buffer.GetBufferMemory(), 0, size, 0, &mappedData) != VK_SUCCESS) {
        return false;
    }
    
    memcpy(mappedData, data, static_cast<size_t>(size));
    vkUnmapMemory(m_device->GetDevice(), buffer.GetBufferMemory());
    
    return true;
}

bool VulkanTexture::CreateAndSetupImage(VkBuffer stagingBuffer, VkCommandPool commandPool, VkQueue graphicsQueue)
{
    // Create image
    if (!CreateImage(p_width, p_height, m_format, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
        return false;
    }
    
    // Transition image layout and copy buffer to image
    TransitionImageLayout(commandPool, graphicsQueue, VK_IMAGE_LAYOUT_UNDEFINED, 
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(commandPool, graphicsQueue, stagingBuffer, p_width, p_height);
    TransitionImageLayout(commandPool, graphicsQueue, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    
    // Create image view and sampler
    if (!CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT)) {
        Cleanup();
        return false;
    }
    
    if (!CreateSampler()) {
        Cleanup();
        return false;
    }
    
    return true;
}

#endif