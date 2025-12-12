#pragma once
#include "VulkanDevice.h"
#ifdef RENDER_API_VULKAN
#include "EngineAPI.h"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

class VulkanDevice;
class Window;

class ENGINE_API VulkanSwapChain
{
public:
    VulkanSwapChain() = default;
    ~VulkanSwapChain();

    bool Initialize(VulkanDevice* device, VkSurfaceKHR surface, Window* window);
    void Cleanup();
    void Recreate(Window* window);

    VkSwapchainKHR GetSwapChain() const { return m_swapChain; }
    VkFormat GetImageFormat() const { return m_imageFormat; }
    VkExtent2D GetExtent() const { return m_extent; }
    const std::vector<VkImage>& GetImages() const { return m_images; }
    const std::vector<VkImageView>& GetImageViews() const { return m_imageViews; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(m_images.size()); }

    VkResult AcquireNextImage(VkSemaphore semaphore, uint32_t* imageIndex);
    VkResult PresentImage(VulkanQueue& presentQueue, uint32_t imageIndex, VkSemaphore waitSemaphore);

private:
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window* window);

    void CreateSwapChain(Window* window);
    void CreateImageViews();
    void CleanupSwapChain();

    VulkanDevice* m_device = nullptr;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat m_imageFormat;
    VkExtent2D m_extent;
};

#endif