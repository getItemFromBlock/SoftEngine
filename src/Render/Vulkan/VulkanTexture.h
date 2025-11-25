#pragma once
#include "VulkanBuffer.h"
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <string>

#include "Render/RHI/RHITexture.h"
#include "Resource/Loader/ImageLoader.h"

class VulkanDevice;

class VulkanTexture : public RHITexture
{
public:
    VulkanTexture() = default;
    VulkanTexture& operator=(const VulkanTexture& other) = default;
    VulkanTexture(const VulkanTexture&) = default;
    VulkanTexture(VulkanTexture&&) noexcept = default;
    ~VulkanTexture() override;
    
 
    bool LoadFromFile(VulkanDevice* device, const std::string& filepath,
                      VkCommandPool commandPool, VkQueue graphicsQueue);

    bool CreateFromImage(const ImageLoader::Image& image, VulkanDevice* device,
                       VkCommandPool commandPool, VkQueue graphicsQueue);

    bool Create(VulkanDevice* device, uint32_t width, uint32_t height,
                VkFormat format, VkImageUsageFlags usage, VkCommandPool commandPool,
                VkQueue graphicsQueue);

    void Cleanup();

    VkImage GetImage() const { return m_image; }
    VkImageView GetImageView() const { return m_imageView; }
    VkSampler GetSampler() const { return m_sampler; }
    VkFormat GetFormat() const { return m_format; }

private:
    bool CreateImage(uint32_t width, uint32_t height, VkFormat format,
                     VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags properties);
    bool CreateImageView(VkImageAspectFlags aspectFlags);
    bool CreateSampler();
    void TransitionImageLayout(VkCommandPool commandPool, VkQueue graphicsQueue,
                               VkImageLayout oldLayout, VkImageLayout newLayout);
    void CopyBufferToImage(VkCommandPool commandPool, VkQueue graphicsQueue,
                           VkBuffer buffer, uint32_t width, uint32_t height);
    bool CopyDataToBuffer(VulkanBuffer& buffer, const void* data, VkDeviceSize size);
    bool CreateAndSetupImage(VkBuffer stagingBuffer, VkCommandPool commandPool, VkQueue graphicsQueue);

    VulkanDevice* m_device = nullptr;
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
    VkImageView m_imageView = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkFormat m_format = VK_FORMAT_R8G8B8A8_SRGB;
};

#endif
