#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <galaxymath/Maths.h>

#include "Utils/Event.h"

class VulkanDevice;

struct GBufferAttachment
{
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

class VulkanGBuffer
{
public:
    static constexpr VkFormat kPositionFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr VkFormat kNormalFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    static constexpr VkFormat kAlbedoFormat = VK_FORMAT_R8G8B8A8_UNORM;
    static constexpr VkFormat kMetallicRoughnessFormat = VK_FORMAT_R8G8B8A8_UNORM;

    ~VulkanGBuffer();

    bool Initialize(VulkanDevice* device, uint32_t width, uint32_t height);
    void Cleanup();

    bool Resize(uint32_t width, uint32_t height);

    const GBufferAttachment& GetPosition() const { return m_position; }
    const GBufferAttachment& GetNormal() const { return m_normal; }
    const GBufferAttachment& GetAlbedo() const { return m_albedo; }
    const GBufferAttachment& GetMetallicRoughness() const { return m_metallicRoughness; }

    VkSampler GetSampler() const { return m_sampler; }

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    bool IsFirstUse() const { return m_firstUse; }
    void MarkUsed() { m_firstUse = false; }

private:
    bool CreateAttachment(GBufferAttachment& attachment, VkFormat format) const;
    void DestroyAttachment(GBufferAttachment& attachment);
    bool CreateSampler();

    VulkanDevice* m_device = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    bool m_firstUse = true;

    GBufferAttachment m_position;
    GBufferAttachment m_normal;
    GBufferAttachment m_albedo;
    GBufferAttachment m_metallicRoughness;

    VkSampler m_sampler = VK_NULL_HANDLE;
};
