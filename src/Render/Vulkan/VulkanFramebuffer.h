#pragma once
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <vector>

class VulkanDevice;

class VulkanFramebuffer
{
public:
    VulkanFramebuffer() = default;
    ~VulkanFramebuffer() = default;

    bool Initialize(VulkanDevice* device, VkRenderPass renderPass,
                   const std::vector<VkImageView>& imageViews, VkExtent2D extent);
    void Cleanup();

    const std::vector<VkFramebuffer>& GetFramebuffers() const { return m_framebuffers; }
    VkFramebuffer GetFramebuffer(size_t index) const { return m_framebuffers[index]; }
    size_t GetCount() const { return m_framebuffers.size(); }

private:
    VulkanDevice* m_device = nullptr;
    std::vector<VkFramebuffer> m_framebuffers;
};

#endif