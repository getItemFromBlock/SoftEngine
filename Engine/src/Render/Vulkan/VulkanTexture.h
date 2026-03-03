#pragma once
#include "VulkanBuffer.h"
#include "VulkanDevice.h"

struct TextureParam;
struct GBufferAttachment;

namespace ImageLoader
{
    struct HDRImage;
    struct Image;
}

class VulkanRenderer;
class VulkanCommandPool;
class VulkanDevice;

class VulkanTexture
{
public:
    VulkanTexture() = default;
    VulkanTexture& operator=(const VulkanTexture& other) = default;
    VulkanTexture(const VulkanTexture&) = default;
    VulkanTexture(VulkanTexture&&) noexcept = default;
    virtual ~VulkanTexture();

    bool CreateFromImage(const ImageLoader::Image& image, VulkanDevice* device, VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue, const TextureParam& param);
    bool CreateRenderTarget(const VkImageCreateInfo& imageInfo, VulkanDevice* device, VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue);
    bool CreateCubemapFromHDR(const ImageLoader::HDRImage& hdr, VulkanDevice* device, VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue);
    bool CreateCubemapWithMips(int resolution, int mipLevels, VulkanDevice* device, VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue);

    void CreateFromGBuffer(const GBufferAttachment& attachment, VkSampler sampler, uint32_t width, uint32_t height);
    
    bool Create(VulkanDevice* device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage);
    void GenerateMipmaps(VulkanCommandPool* pool, VulkanQueue& queue) const;
    void Cleanup();

    VkImageView GetMipLevelView(int mipLevel) const;

    void SetPreferredFilterType(VkFilter filterType) { m_preferredFilter = filterType; }

    VkImage GetImage() const { return m_image; }
    VkImageView GetImageView() const { return m_imageView; }
    VkSampler GetSampler() const { return m_sampler; }
    VkFormat GetFormat() const { return m_format; }

private:
    // Texture2D
    bool CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels = 1);
    bool CreateImageView(VkImageAspectFlags aspectFlags);
    bool CreateSampler();
    void CopyBufferToImage(VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue,
                           VkBuffer buffer, uint32_t width, uint32_t height) const;
    bool CopyDataToBuffer(VulkanBuffer& buffer, const void* data, VkDeviceSize size) const;
    bool CreateAndSetupImage(VkBuffer stagingBuffer, VulkanCommandPool* commandPool,
                             VulkanQueue& graphicsQueue);

    // Cubemap
    bool ConvertEquirectangularToCubemap(const ImageLoader::HDRImage& hdr,
                                                float* cubemapData, uint32_t faceSize);
    bool CreateAndSetupCubemap(VkBuffer stagingBuffer, uint32_t faceSize,
                               VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue);
    void CreateCubemapImageView(VkFormat format, uint32_t mipLevels = 1);
    void CreateCubemapSampler();
    void CreateCubemapSamplerWithMips(uint32_t mipLevels);
    void TransitionImageLayoutWithMips(VkImage image,
                                       VkImageLayout oldLayout, VkImageLayout newLayout,
                                       uint32_t mipLevels, uint32_t layerCount,
                                       VulkanCommandPool* commandPool, VulkanQueue& graphicsQueue) const;

    // Single-time command helpers
    void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height,
                           uint32_t layerCount, VulkanCommandPool* commandPool,
                           VulkanQueue& graphicsQueue) const;
    VkCommandBuffer BeginSingleTimeCommands(VulkanCommandPool* commandPool) const;
    void EndSingleTimeCommands(VkCommandBuffer commandBuffer, VulkanCommandPool* commandPool,
                               VulkanQueue& graphicsQueue) const;

private:
    VulkanDevice* m_device = nullptr;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkFilter m_preferredFilter = VK_FILTER_LINEAR;
    VkFormat m_format = VK_FORMAT_R8G8B8A8_SRGB;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_mipLevels = 1;

    std::vector<VkImageView> m_mipLevelViews = {};
};
