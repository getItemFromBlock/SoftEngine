#pragma once
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <vector>

class VulkanDevice;

class VulkanSyncObjects
{
public:
    VulkanSyncObjects() = default;
    ~VulkanSyncObjects();

    bool Initialize(VulkanDevice* device, uint32_t maxFramesInFlight);
    void Cleanup();

    VkSemaphore GetImageAvailableSemaphore(size_t index) const { return m_imageAvailableSemaphores[index]; }
    VkSemaphore GetRenderFinishedSemaphore(size_t index) const { return m_renderFinishedSemaphores[index]; }
    VkFence GetInFlightFence(size_t index) const { return m_inFlightFences[index]; }

    void WaitForFence(size_t index);
    void ResetFence(size_t index);
    
    bool ResizeRenderFinishedSemaphores(uint32_t swapChainImageCount);
private:
    VulkanDevice* m_device = nullptr;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
};

#endif