#include "VulkanTexture.h"

#include <stdexcept>

#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanUtils.h"
#include "VulkanCommandPool.h"
#include "VulkanGBuffer.h"

#include "Debug/Log.h"
#include "Resource/Texture.h"
#include "Resource/Loader/ImageLoader.h"

VulkanTexture::~VulkanTexture()
{
    Cleanup();
}

void VulkanTexture::Cleanup()
{
    if (!m_device)
        return;

    VkDevice device = m_device->GetDevice();

    for (VkImageView mipView : m_mipLevelViews)
    {
        if (mipView != VK_NULL_HANDLE)
            vkDestroyImageView(device, mipView, nullptr);
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

bool VulkanTexture::CreateFromImage(const ImageLoader::Image& image, VulkanDevice* device,
                                    VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue,
                                    const TextureParam& param)
{
    if (!device)
        return false;

    switch (param.format)
    {
    case TextureFormat::SRGB:
        m_format = VK_FORMAT_R8G8B8A8_SRGB;
        break;
    case TextureFormat::UNORM:
        m_format = VK_FORMAT_R8G8B8A8_UNORM;
        break;
    case TextureFormat::R8_UNORM:
        m_format = VK_FORMAT_R8_UNORM;
        break;
    }

    m_device = device;
    switch (param.filter)
    {
    case TextureFilter::LINEAR:
        m_preferredFilter = VK_FILTER_LINEAR;
        break;
    case TextureFilter::NEAREST:
        m_preferredFilter = VK_FILTER_NEAREST;
        break;
    }


    m_width = image.size.x;
    m_height = image.size.y;
    m_mipLevels = static_cast<uint32_t>(
        std::floor(std::log2(std::max(m_width, m_height)))) + 1;

    const VkDeviceSize imageSize = m_width * m_height * 4;

    VulkanBuffer stagingBuffer;
    if (!stagingBuffer.Initialize(m_device, imageSize,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        return false;

    if (!CopyDataToBuffer(stagingBuffer, image.data, imageSize))
        return false;

    return CreateAndSetupImage(stagingBuffer.GetBuffer(), commandPool, graphicsQueue);
}

bool VulkanTexture::Create(VulkanDevice* device, uint32_t width, uint32_t height,
                           VkFormat format, VkImageUsageFlags usage)
{
    if (!device || width == 0 || height == 0)
        return false;

    m_device = device;
    m_width = width;
    m_height = height;
    m_format = format;
    m_mipLevels = 1;

    if (!CreateImage(width, height, format, VK_IMAGE_TILING_OPTIMAL, usage,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_mipLevels))
        return false;

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

bool VulkanTexture::CreateRenderTarget(const VkImageCreateInfo& imageInfo, VulkanDevice* device,
                                       VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue)
{
    if (!device)
        return false;

    m_device = device;
    m_width = imageInfo.extent.width;
    m_height = imageInfo.extent.height;
    m_format = imageInfo.format;

    if (vkCreateImage(m_device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_device->GetDevice(), m_image, &memReq);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(memReq.memoryTypeBits,
                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
    {
        vkDestroyImage(m_device->GetDevice(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(m_device->GetDevice(), m_image, m_imageMemory, 0);

    // Transition to shader-readable layout
    VkCommandBuffer cmd = BeginSingleTimeCommands(commandPool);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    EndSingleTimeCommands(cmd, commandPool, graphicsQueue);

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
        return false;

    m_device = device;

    const uint32_t faceSize = std::min(static_cast<uint32_t>(hdr.size.x) / 4, 2048u);
    m_width = faceSize;
    m_height = faceSize;

    constexpr uint32_t numFaces = 6;
    const VkDeviceSize imageSize = faceSize * faceSize * 4 * sizeof(float) * numFaces;

    VulkanBuffer stagingBuffer;
    if (!stagingBuffer.Initialize(m_device, imageSize,
                                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        return false;

    std::vector<float> cubemapData(faceSize * faceSize * 4 * numFaces);
    if (!ConvertEquirectangularToCubemap(hdr, cubemapData.data(), faceSize))
        return false;

    if (!CopyDataToBuffer(stagingBuffer, cubemapData.data(), imageSize))
        return false;

    return CreateAndSetupCubemap(stagingBuffer.GetBuffer(), faceSize, commandPool, graphicsQueue);
}

bool VulkanTexture::ConvertEquirectangularToCubemap(const ImageLoader::HDRImage& hdr,
                                                    float* cubemapData,
                                                    uint32_t faceSize)
{
    struct CubeFace
    {
        Vec3f right, up, forward;
    };

    const CubeFace faces[6] = {
        {Vec3f(0, 0, -1), Vec3f(0, -1, 0), Vec3f(-1, 0, 0)}, // +X
        {Vec3f(0, 0, 1), Vec3f(0, -1, 0), Vec3f(1, 0, 0)}, // -X
        {Vec3f(-1, 0, 0), Vec3f(0, 0, 1), Vec3f(0, 1, 0)}, // +Y
        {Vec3f(-1, 0, 0), Vec3f(0, 0, -1), Vec3f(0, -1, 0)}, // -Y
        {Vec3f(-1, 0, 0), Vec3f(0, -1, 0), Vec3f(0, 0, 1)}, // +Z
        {Vec3f(1, 0, 0), Vec3f(0, -1, 0), Vec3f(0, 0, -1)}, // -Z
    };

    for (uint32_t face = 0; face < 6; ++face)
    {
        for (uint32_t y = 0; y < faceSize; ++y)
        {
            for (uint32_t x = 0; x < faceSize; ++x)
            {
                const float u = (2.0f * (x + 0.5f) / faceSize) - 1.0f;
                const float v = (2.0f * (y + 0.5f) / faceSize) - 1.0f;

                const Vec3f dir = Vec3f::Normalize(
                    faces[face].forward + u * faces[face].right + v * faces[face].up);

                const float theta = std::atan2f(dir.z, dir.x);
                const float phi = std::asinf(dir.y);

                const float texU = (theta / (2.0f * PI)) + 0.5f;
                const float texV = (phi / PI) + 0.5f;

                int hdrX = static_cast<int>(texU * hdr.size.x) % hdr.size.x;
                int hdrY = std::clamp(static_cast<int>(texV * hdr.size.y), 0, hdr.size.y - 1);
                if (hdrX < 0) hdrX += hdr.size.x;

                const int hdrIdx = (hdrY * hdr.size.x + hdrX) * hdr.channels;
                const int outIdx = static_cast<int>((face * faceSize * faceSize + y * faceSize + x) * 4);

                cubemapData[outIdx + 0] = hdr.data[hdrIdx + 0];
                cubemapData[outIdx + 1] = hdr.data[hdrIdx + 1];
                cubemapData[outIdx + 2] = hdr.data[hdrIdx + 2];
                cubemapData[outIdx + 3] = 1.0f;
            }
        }
    }

    return true;
}

bool VulkanTexture::CreateAndSetupCubemap(VkBuffer stagingBuffer, uint32_t faceSize,
                                          VulkanCommandPool* commandPool,
                                          VulkanQueue& graphicsQueue)
{
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {faceSize, faceSize, 1};
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
        return false;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_device->GetDevice(), m_image, &memReq);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(memReq.memoryTypeBits,
                                                         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(m_device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
        return false;

    vkBindImageMemory(m_device->GetDevice(), m_image, m_imageMemory, 0);

    VulkanUtils::TransitionImageLayout(commandPool, graphicsQueue,
                                       VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_device,
                                       m_image, 6);

    CopyBufferToImage(stagingBuffer, m_image, faceSize, faceSize, 6, commandPool, graphicsQueue);

    VulkanUtils::TransitionImageLayout(commandPool, graphicsQueue,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                       m_device, m_image, 6);

    CreateCubemapImageView(VK_FORMAT_R32G32B32A32_SFLOAT, 1);
    CreateCubemapSampler();

    return true;
}

bool VulkanTexture::CreateCubemapWithMips(int resolution, int mipLevels,
                                          VulkanDevice* device,
                                          VulkanCommandPool* commandPool,
                                          VulkanQueue& graphicsQueue)
{
    if (!device)
        return false;

    m_device = device;
    m_width = resolution;
    m_height = resolution;
    m_mipLevels = mipLevels;

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {static_cast<uint32_t>(resolution), static_cast<uint32_t>(resolution), 1};
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device->GetDevice(), m_image, &memReq);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = device->FindMemoryType(memReq.memoryTypeBits,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
    {
        vkDestroyImage(device->GetDevice(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(device->GetDevice(), m_image, m_imageMemory, 0);

    TransitionImageLayoutWithMips(m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_GENERAL, mipLevels,
                                  6, commandPool, graphicsQueue);

    CreateCubemapImageView(VK_FORMAT_R16G16B16A16_SFLOAT, mipLevels);

    // Per-mip views (for compute write)
    m_mipLevelViews.resize(mipLevels, VK_NULL_HANDLE);
    for (int mip = 0; mip < mipLevels; ++mip)
    {
        VkImageViewCreateInfo mipViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        mipViewInfo.image = m_image;
        mipViewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        mipViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        mipViewInfo.subresourceRange = {
            VK_IMAGE_ASPECT_COLOR_BIT,
            static_cast<uint32_t>(mip), 1, 0, 6
        };

        if (vkCreateImageView(device->GetDevice(), &mipViewInfo, nullptr,
                              &m_mipLevelViews[mip]) != VK_SUCCESS)
        {
            Cleanup();
            return false;
        }
    }

    CreateCubemapSamplerWithMips(mipLevels);

    return true;
}

bool VulkanTexture::CreateCubemap(uint32_t resolution, VulkanDevice* device,
                                  VulkanCommandPool* commandPool,
                                  VulkanQueue& graphicsQueue)
{
    if (!device)
        return false;

    m_device = device;
    m_width = resolution;
    m_height = resolution;
    m_mipLevels = 1;

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {resolution, resolution, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 6;
    imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    if (vkCreateImage(device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device->GetDevice(), m_image, &memReq);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = device->FindMemoryType(memReq.memoryTypeBits,
                                                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device->GetDevice(), &allocInfo, nullptr, &m_imageMemory) != VK_SUCCESS)
    {
        vkDestroyImage(device->GetDevice(), m_image, nullptr);
        m_image = VK_NULL_HANDLE;
        return false;
    }

    vkBindImageMemory(device->GetDevice(), m_image, m_imageMemory, 0);

    TransitionImageLayoutWithMips(m_image, VK_IMAGE_LAYOUT_UNDEFINED,
                                  VK_IMAGE_LAYOUT_GENERAL,
                                  1, 6, commandPool, graphicsQueue);

    CreateCubemapImageView(VK_FORMAT_R16G16B16A16_SFLOAT, 1);

    m_mipLevelViews.resize(1, VK_NULL_HANDLE);

    VkImageViewCreateInfo storageViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    storageViewInfo.image = m_image;
    storageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    storageViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    storageViewInfo.subresourceRange = {
        VK_IMAGE_ASPECT_COLOR_BIT,
        0, 1,
        0, 6
    };

    if (vkCreateImageView(device->GetDevice(), &storageViewInfo, nullptr,
                          &m_mipLevelViews[0]) != VK_SUCCESS)
    {
        Cleanup();
        return false;
    }

    CreateCubemapSampler();

    return true;
}

void VulkanTexture::CreateFromGBuffer(const GBufferAttachment& attachment, VkSampler sampler, uint32_t width,
                                      uint32_t height)
{
    m_image = attachment.image;
    m_imageView = attachment.imageView;
    m_imageMemory = attachment.memory;
    m_format = attachment.format;
    m_width = width;
    m_height = height;
    m_sampler = sampler;
}

VkImageView VulkanTexture::GetMipLevelView(int mipLevel) const
{
    if (mipLevel >= 0 && mipLevel < static_cast<int>(m_mipLevelViews.size()))
        return m_mipLevelViews[mipLevel];
    return VK_NULL_HANDLE;
}

void VulkanTexture::GenerateMipmaps(VulkanCommandPool* pool, VulkanQueue& queue) const
{
    VkCommandBuffer cmd = m_device->BeginSingleTimeCommands(pool);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.image = m_image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    int32_t mipWidth = static_cast<int32_t>(m_width);
    int32_t mipHeight = static_cast<int32_t>(m_height);

    for (uint32_t i = 1; i < m_mipLevels; ++i)
    {
        // Transition previous level to TRANSFER_SRC
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        const int32_t nextWidth = mipWidth > 1 ? mipWidth / 2 : 1;
        const int32_t nextHeight = mipHeight > 1 ? mipHeight / 2 : 1;

        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {nextWidth, nextHeight, 1};

        vkCmdBlitImage(cmd,
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);

        // Transition i-1 to SHADER_READ_ONLY
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        mipWidth = nextWidth;
        mipHeight = nextHeight;
    }

    // Transition the last mip level
    barrier.subresourceRange.baseMipLevel = m_mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_device->EndSingleTimeCommands(pool, queue, cmd);
}

void VulkanTexture::CreateCubemapImageView(VkFormat format, uint32_t mipLevels)
{
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    viewInfo.format = format;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, 6};

    vkCreateImageView(m_device->GetDevice(), &viewInfo, nullptr, &m_imageView);
}

void VulkanTexture::CreateCubemapSampler()
{
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    if (vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create cubemap sampler!");
}

void VulkanTexture::CreateCubemapSamplerWithMips(uint32_t mipLevels)
{
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(mipLevels);

    if (vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create cubemap sampler with mips!");
}

void VulkanTexture::TransitionImageLayoutWithMips(VkImage image,
                                                  VkImageLayout oldLayout, VkImageLayout newLayout,
                                                  uint32_t mipLevels, uint32_t layerCount,
                                                  VulkanCommandPool* commandPool,
                                                  VulkanQueue& graphicsQueue) const
{
    VkCommandBuffer cmd = BeginSingleTimeCommands(commandPool);

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, mipLevels, 0, layerCount};

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_GENERAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL &&
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition in TransitionImageLayoutWithMips!");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    EndSingleTimeCommands(cmd, commandPool, graphicsQueue);
}

bool VulkanTexture::CreateImage(uint32_t width, uint32_t height, VkFormat format,
                                VkImageTiling tiling, VkImageUsageFlags usage,
                                VkMemoryPropertyFlags properties, uint32_t mipLevels)
{
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(m_device->GetDevice(), &imageInfo, nullptr, &m_image) != VK_SUCCESS)
        return false;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(m_device->GetDevice(), m_image, &memReq);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = m_device->FindMemoryType(memReq.memoryTypeBits, properties);

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
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = m_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_format;
    // Expose all mip levels so the sampler can access them
    viewInfo.subresourceRange = {aspectFlags, 0, m_mipLevels, 0, 1};

    return vkCreateImageView(m_device->GetDevice(), &viewInfo, nullptr, &m_imageView) == VK_SUCCESS;
}

bool VulkanTexture::CreateSampler()
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_device->GetPhysicalDevice(), &props);

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = m_preferredFilter;
    samplerInfo.minFilter = m_preferredFilter;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = props.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(m_mipLevels);

    return vkCreateSampler(m_device->GetDevice(), &samplerInfo, nullptr, &m_sampler) == VK_SUCCESS;
}

bool VulkanTexture::CreateAndSetupImage(VkBuffer stagingBuffer,
                                        VulkanCommandPool* commandPool,
                                        VulkanQueue& graphicsQueue)
{
    if (!CreateImage(m_width, m_height, m_format, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                     m_mipLevels))
        return false;

    {
        VkCommandBuffer cmd = BeginSingleTimeCommands(commandPool);

        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, m_mipLevels, 0, 1};
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndSingleTimeCommands(cmd, commandPool, graphicsQueue);
    }

    CopyBufferToImage(commandPool, graphicsQueue, stagingBuffer, m_width, m_height);

    GenerateMipmaps(commandPool, graphicsQueue);

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

void VulkanTexture::CopyBufferToImage(VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue,
                                      VkBuffer buffer, uint32_t width, uint32_t height) const
{
    VkCommandBuffer cmd = m_device->BeginSingleTimeCommands(commandPool);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(cmd, buffer, m_image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    m_device->EndSingleTimeCommands(commandPool, graphicsQueue, cmd);
}

void VulkanTexture::CopyBufferToImage(VkBuffer buffer, VkImage image,
                                      uint32_t width, uint32_t height,
                                      uint32_t layerCount,
                                      VulkanCommandPool* commandPool,
                                      VulkanQueue& graphicsQueue) const
{
    VkCommandBuffer cmd = BeginSingleTimeCommands(commandPool);

    std::vector<VkBufferImageCopy> regions(layerCount);
    for (uint32_t layer = 0; layer < layerCount; ++layer)
    {
        VkBufferImageCopy& region = regions[layer];
        region.bufferOffset = static_cast<VkDeviceSize>(layer) * width * height * 4 * sizeof(float);
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
    }

    vkCmdCopyBufferToImage(cmd, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           static_cast<uint32_t>(regions.size()), regions.data());

    EndSingleTimeCommands(cmd, commandPool, graphicsQueue);
}

bool VulkanTexture::CopyDataToBuffer(VulkanBuffer& buffer, const void* data, VkDeviceSize size) const
{
    void* mapped;
    if (vkMapMemory(m_device->GetDevice(), buffer.GetBufferMemory(), 0, size, 0, &mapped) != VK_SUCCESS)
        return false;

    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(m_device->GetDevice(), buffer.GetBufferMemory());
    return true;
}

VkCommandBuffer VulkanTexture::BeginSingleTimeCommands(VulkanCommandPool* commandPool) const
{
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool->GetCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device->GetDevice(), &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    return cmd;
}

void VulkanTexture::EndSingleTimeCommands(VkCommandBuffer commandBuffer,
                                          VulkanCommandPool* commandPool,
                                          VulkanQueue& graphicsQueue) const
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue.handle, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue.handle);

    vkFreeCommandBuffers(m_device->GetDevice(), commandPool->GetCommandPool(), 1, &commandBuffer);
}
