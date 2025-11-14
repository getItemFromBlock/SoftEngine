#pragma once
#ifdef RENDER_API_VULKAN
#include <vulkan/vulkan_core.h>
#include <optional>
#include <vector>

class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice() = default;

    bool Initialize(VkInstance instance, VkSurfaceKHR surface);
    void Cleanup();

    // Getters
    VkDevice GetDevice() const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetPresentQueue() const { return m_presentQueue; }
    uint32_t GetGraphicsQueueFamily() const { return m_queueFamilies.graphicsFamily.value(); }
    uint32_t GetPresentQueueFamily() const { return m_queueFamilies.presentFamily.value(); }

    // Utility
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };
    
    QueueFamilyIndices GetQueueFamilyIndices() const { return m_queueFamilies; }

private:
    bool PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface);
    int RateDeviceSuitability(VkPhysicalDevice device) const;
    bool IsDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) const;
    void CreateLogicalDevice(VkSurfaceKHR surface);

private:
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    QueueFamilyIndices m_queueFamilies;

    std::vector<const char*> m_enabledDeviceExtensions;
};
#endif