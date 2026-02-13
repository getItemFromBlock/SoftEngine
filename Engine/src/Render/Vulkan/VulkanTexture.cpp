#include "VulkanTexture.h"

#include <array>

#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include <stb_image.h>
#include <stdexcept>
#include <cstring>

#include "VulkanCommandPool.h"
#include "Debug/Log.h"
#include "Resource/Loader/ImageLoader.h"

VulkanTexture::~VulkanTexture()
{
    Cleanup();
}

bool VulkanTexture::CreateFromImage(const ImageLoader::Image& image, VulkanDevice* device,
                                    VulkanCommandPool* commandBuffer, VulkanQueue& graphicsQueue)
{
    if (!device)
    {
        return false;
    }

    m_device = device;
    p_width = image.size.x;
    p_height = image.size.y;
    auto pixels = image.data;
    VkDeviceSize imageSize = p_width * p_height * 4;

    VulkanBuffer stagingBuffer;
    if (!stagingBuffer.Initialize(m_device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        return false;
    }
    if (!CopyDataToBuffer(stagingBuffer, pixels, imageSize))
    {
        return false;
    }

    if (!CreateAndSetupImage(stagingBuffer.GetBuffer(), commandBuffer, graphicsQueue))
    {
        return false;
    }

    return true;
}

bool VulkanTexture::CreateRenderTarget(const VkImageCreateInfo& imageInfo, VulkanDevice* device,
                                       VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue)
{
    if (!device)
    {
        return false;
    }

    m_device = device;
    p_width = imageInfo.extent.width;
    p_height = imageInfo.extent.height;
    m_format = imageInfo.format;

    if (vkCreateImage(m_device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device->GetDevice(), m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(memRequirements.memoryTypeBits,
                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
    {
        vkDestroyImage(m_device->GetDevice(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(m_device->GetDevice(), m_image, m_imageMemory, 0);

    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    EndSingleTimeCommands(commandBuffer, commandPool, graphicsQueue);

    if (!CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT))
    {
        Cleanup();
        return false;
    }

    if (!CreateSampler())
    {
        Cleanup();
        return false;
    }

    return true;
}

bool VulkanTexture::CreateCubemapFromHDR(const ImageLoader::HDRImage& hdr,
                                         VulkanDevice* device,
                                         VulkanCommandPool* commandPool,
                                         VulkanQueue& graphicsQueue)
{
    if (!device || !hdr.data)
    {
        return false;
    }

    m_device = device;

    uint32_t faceSize = std::min(static_cast<uint32_t>(hdr.size.x) / 4, 2048u);

    p_width = faceSize;
    p_height = faceSize;

    const uint32_t numFaces = 6;
    VkDeviceSize imageSize = faceSize * faceSize * 4 * sizeof(float) * numFaces;

    VulkanBuffer stagingBuffer;
    if (!stagingBuffer.Initialize(m_device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
    {
        return false;
    }

    std::vector<float> cubemapData(faceSize * faceSize * 4 * numFaces);
    if (!ConvertEquirectangularToCubemap(hdr, cubemapData.data(), faceSize))
    {
        return false;
    }

    if (!CopyDataToBuffer(stagingBuffer, cubemapData.data(), imageSize))
    {
        return false;
    }

    if (!CreateAndSetupCubemap(stagingBuffer.GetBuffer(), faceSize, commandPool, graphicsQueue))
    {
        return false;
    }

    return true;
}

bool VulkanTexture::ConvertEquirectangularToCubemap(const ImageLoader::HDRImage& hdr,
                                                    float* cubemapData,
                                                    uint32_t faceSize)
{
    struct CubeFace
    {
        Vec3f right, up, forward;
    };

    CubeFace faces[6] = {
        {Vec3f(0, 0, -1), Vec3f(0, -1, 0), Vec3f(-1, 0, 0)},
        {Vec3f(0, 0, 1), Vec3f(0, -1, 0), Vec3f(1, 0, 0)},
        {Vec3f(-1, 0, 0), Vec3f(0, 0, 1), Vec3f(0, 1, 0)},
        {Vec3f(-1, 0, 0), Vec3f(0, 0, -1), Vec3f(0, -1, 0)},
        {Vec3f(-1, 0, 0), Vec3f(0, -1, 0), Vec3f(0, 0, 1)},
        {Vec3f(1, 0, 0), Vec3f(0, -1, 0), Vec3f(0, 0, -1)}
    };

    for (uint32_t face = 0; face < 6; ++face)
    {
        for (uint32_t y = 0; y < faceSize; ++y)
        {
            for (uint32_t x = 0; x < faceSize; ++x)
            {
                float u = (2.0f * (x + 0.5f) / faceSize) - 1.0f;
                float v = (2.0f * (y + 0.5f) / faceSize) - 1.0f;

                Vec3f dir = Vec3f::Normalize(
                    faces[face].forward +
                    u * faces[face].right +
                    v * faces[face].up
                );

                float theta = std::atan2f(dir.z, dir.x);
                float phi = std::asinf(dir.y);

                float texU = (theta / (2.0f * PI)) + 0.5f;
                float texV = (phi / PI) + 0.5f;

                int hdrX = static_cast<int>(texU * hdr.size.x) % hdr.size.x;
                int hdrY = std::clamp(static_cast<int>(texV * hdr.size.y), 0, static_cast<int>(hdr.size.y - 1));

                if (hdrX < 0) hdrX += hdr.size.x;

                int hdrIndex = (hdrY * hdr.size.x + hdrX) * hdr.channels;
                int outIndex = (face * faceSize * faceSize + y * faceSize + x) * 4;

                cubemapData[outIndex + 0] = hdr.data[hdrIndex + 0];
                cubemapData[outIndex + 1] = hdr.data[hdrIndex + 1];
                cubemapData[outIndex + 2] = hdr.data[hdrIndex + 2];
                cubemapData[outIndex + 3] = 1.0f;
            }
        }
    }

    return true;
}

bool VulkanTexture::CreateAndSetupCubemap(VkBuffer stagingBuffer,
                                          uint32_t faceSize,
                                          VulkanCommandPool* commandPool,
                                          VulkanQueue& graphicsQueue)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = faceSize;
    imageInfo.extent.height = faceSize;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(m_device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device->GetDevice(), m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(memRequirements.memoryTypeBits,
                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
    {
        return false;
    }

    vkBindImageMemory(m_device->GetDevice(), m_image, m_imageMemory, 0);

    TransitionImageLayout(m_image, VK_FORMAT_R32G32B32A32_SFLOAT,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          6, commandPool, graphicsQueue);

    CopyBufferToImage(stagingBuffer, m_image, faceSize, faceSize, 6, commandPool, graphicsQueue);

    TransitionImageLayout(m_image, VK_FORMAT_R32G32B32A32_SFLOAT,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          6, commandPool, graphicsQueue);

    CreateCubemapImageView(VK_FORMAT_R32G32B32A32_SFLOAT);

    CreateCubemapSampler();

    return true;
}

void VulkanTexture::CreateCubemapImageView(VkFormat format)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    vkCreateImageView(m_device->GetDevice(), &viewInfo, nullptr, &m_imageView);
}

void VulkanTexture::CreateCubemapSampler()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create cubemap sampler!");
    }
}

VkCommandBuffer VulkanTexture::BeginSingleTimeCommands(VulkanCommandPool* commandPool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool->GetCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_device->GetDevice(), &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanTexture::EndSingleTimeCommands(VkCommandBuffer commandBuffer,
                                          VulkanCommandPool* commandPool,
                                          VulkanQueue& graphicsQueue)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue.handle, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue.handle);

    vkFreeCommandBuffers(m_device->GetDevice(), commandPool->GetCommandPool(), 1, &commandBuffer);
}

void VulkanTexture::TransitionImageLayout(VkImage image, VkFormat format,
                                          VkImageLayout oldLayout, VkImageLayout newLayout,
                                          uint32_t layerCount,
                                          VulkanCommandPool* commandPool,
                                          VulkanQueue& graphicsQueue)
{
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    EndSingleTimeCommands(commandBuffer, commandPool, graphicsQueue);
}

void VulkanTexture::CopyBufferToImage(VkBuffer buffer, VkImage image,
                                      uint32_t width, uint32_t height,
                                      uint32_t layerCount,
                                      VulkanCommandPool* commandPool,
                                      VulkanQueue& graphicsQueue)
{
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

    std::vector<VkBufferImageCopy> regions(layerCount);

    for (uint32_t layer = 0; layer < layerCount; ++layer)
    {
        VkBufferImageCopy& region = regions[layer];
        region.bufferOffset = layer * width * height * 4 * sizeof(float);
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = layer;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
    }

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(regions.size()),
        regions.data()
    );

    EndSingleTimeCommands(commandBuffer, commandPool, graphicsQueue);
}

bool VulkanTexture::Create(VulkanDevice* device, uint32_t width, uint32_t height,
                           VkFormat format, VkImageUsageFlags usage,
                           VkCommandPool commandPool, VkQueue graphicsQueue)
{
    UNUSED(commandPool);
    UNUSED(graphicsQueue);

    if (!device || width == 0 || height == 0)
    {
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

    if (!CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT))
    {
        Cleanup();
        return false;
    }

    if (!CreateSampler())
    {
        Cleanup();
        return false;
    }

    return true;
}

void VulkanTexture::Cleanup()
{
    if (m_device == nullptr) return;

    VkDevice device = m_device->GetDevice();

    for (auto& mipView : m_mipLevelViews)
    {
        if (mipView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(device, mipView, nullptr);
        }
    }
    m_mipLevelViews.clear();

    if (m_sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, m_imageView, nullptr);
        m_imageView = VK_NULL_HANDLE;
    }

    if (m_image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, m_image, nullptr);
        m_image = VK_NULL_HANDLE;
    }

    if (m_imageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, m_imageMemory, nullptr);
        m_imageMemory = VK_NULL_HANDLE;
    }

    m_device = nullptr;
}

bool VulkanTexture::CreateCubemapWithMips(int resolution, int mipLevels, 
                                          VulkanDevice* device,
                                          VulkanCommandPool* commandPool,
                                          VulkanQueue& graphicsQueue)
{
    if (!device)
    {
        return false;
    }

    m_device = device;
    p_width = resolution;
    p_height = resolution;
    m_mipLevels = mipLevels;

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = resolution;
    imageInfo.extent.height = resolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device->GetDevice(), m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = device->FindMemoryType(memRequirements.memoryTypeBits,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
    {
        vkDestroyImage(device->GetDevice(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(device->GetDevice(), m_image, m_imageMemory, 0);

    TransitionImageLayoutWithMips(m_image, VK_FORMAT_R16G16B16A16_SFLOAT,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                  mipLevels, 6, commandPool, graphicsQueue);

    CreateCubemapImageView(VK_FORMAT_R16G16B16A16_SFLOAT, mipLevels);

    m_mipLevelViews.resize(mipLevels);
    for (int mip = 0; mip < mipLevels; ++mip)
    {
        VkImageViewCreateInfo mipViewInfo{};
        mipViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        mipViewInfo.image = m_image;
        mipViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        mipViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        mipViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mipViewInfo.subresourceRange.baseMipLevel = mip;
        mipViewInfo.subresourceRange.levelCount = 1;
        mipViewInfo.subresourceRange.baseArrayLayer = 0;
        mipViewInfo.subresourceRange.layerCount = 6;

        if (vkCreateImageView(device->GetDevice(), &mipViewInfo, nullptr, &m_mipLevelViews[mip]) != VK_SUCCESS)
        {
            Cleanup();
            return false;
        }
    }

    CreateCubemapSamplerWithMips(mipLevels);

    return true;
}

VkImageView VulkanTexture::GetMipLevelView(int mipLevel) const
{
    if (mipLevel >= 0 && mipLevel < static_cast<int>(m_mipLevelViews.size()))
    {
        return m_mipLevelViews[mipLevel];
    }
    return VK_NULL_HANDLE;
}

void VulkanTexture::CreateCubemapImageView(VkFormat format, uint32_t mipLevels)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 6;

    vkCreateImageView(m_device->GetDevice(), &viewInfo, nullptr, &m_imageView);
}

void VulkanTexture::CreateCubemapSamplerWithMips(uint32_t mipLevels)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);

    if (vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create cubemap sampler!");
    }
}

void VulkanTexture::TransitionImageLayoutWithMips(VkImage image, VkFormat format,
                                                   VkImageLayout oldLayout, VkImageLayout newLayout,
                                                   uint32_t mipLevels, uint32_t layerCount,
                                                   VulkanCommandPool* commandPool,
                                                   VulkanQueue& graphicsQueue)
{
    VkCommandBuffer commandBuffer = BeginSingleTimeCommands(commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    EndSingleTimeCommands(commandBuffer, commandPool, graphicsQueue);
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

    if (vkCreateImage(m_device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device->GetDevice(), m_image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
    {
        vkDestroyImage(m_device->GetDevice(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindImageMemory(m_device->GetDevice(), m_image, m_imageMemory, 0) != VK_SUCCESS)
    {
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

    if (vkCreateImageView(m_device->GetDevice(), &viewInfo, nullptr, &m_imageView) != VK_SUCCESS)
    {
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

    if (vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
    {
        return false;
    }

    return true;
}

void VulkanTexture::TransitionImageLayout(VulkanCommandPool* _commandBuffer, VulkanQueue& graphicsQueue,
                                          VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer commandBuffer = m_device->BeginSingleTimeCommands(_commandBuffer);

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

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    m_device->EndSingleTimeCommands(_commandBuffer, graphicsQueue, commandBuffer);
}

void VulkanTexture::CopyBufferToImage(VulkanCommandPool* _commandBuffer, VulkanQueue& graphicsQueue,
                                      VkBuffer buffer, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = m_device->BeginSingleTimeCommands(_commandBuffer);

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

    m_device->EndSingleTimeCommands(_commandBuffer, graphicsQueue, commandBuffer);
}

bool VulkanTexture::CopyDataToBuffer(VulkanBuffer& buffer, const void* data, VkDeviceSize size)
{
    void* mappedData;
    if (vkMapMemory(m_device->GetDevice(), buffer.GetBufferMemory(), 0, size, 0, &mappedData) != VK_SUCCESS)
    {
        return false;
    }

    memcpy(mappedData, data, static_cast<size_t>(size));
    vkUnmapMemory(m_device->GetDevice(), buffer.GetBufferMemory());

    return true;
}

bool VulkanTexture::CreateAndSetupImage(VkBuffer stagingBuffer, VulkanCommandPool* commandBuffer,
                                        VulkanQueue& graphicsQueue)
{
    if (!CreateImage(p_width, p_height, m_format, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
    {
        return false;
    }

    TransitionImageLayout(commandBuffer, graphicsQueue, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(commandBuffer, graphicsQueue, stagingBuffer, p_width, p_height);
    TransitionImageLayout(commandBuffer, graphicsQueue, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    if (!CreateImageView(VK_IMAGE_ASPECT_COLOR_BIT))
    {
        Cleanup();
        return false;
    }

    if (!CreateSampler())
    {
        Cleanup();
        return false;
    }

    return true;
}