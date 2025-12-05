#pragma once
#ifdef RENDER_API_VULKAN
#include <mutex>
#include <vulkan/vulkan_core.h>
#include <optional>
#include <vector>
#include <memory>

class VulkanCommandPool;
class VulkanTexture;

struct VulkanQueue
{
    VulkanQueue(VkQueue _queue) : handle(_queue) {}
    
    VkQueue handle;
    std::shared_ptr<std::mutex> mutex = std::make_shared<std::mutex>();
};


class VulkanDevice {
public:
    VulkanDevice() = default;
    ~VulkanDevice();

    bool Initialize(VkInstance instance, VkSurfaceKHR surface);
    void Cleanup();

    // Getters
    VkDevice GetDevice() const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VulkanQueue& GetGraphicsQueue() { return m_graphicsQueue; }
    VulkanQueue& GetPresentQueue() { return m_presentQueue; }
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
    
    VkCommandBuffer BeginSingleTimeCommands(VulkanCommandPool* commandPool);
    void EndSingleTimeCommands(VulkanCommandPool* commandPool, VulkanQueue& queue, VkCommandBuffer commandBuffer);

    void SetDefaultTexture(VulkanTexture* texture) { m_defaultTexture = texture; }
    VulkanTexture* GetDefaultTexture() const { return m_defaultTexture; }
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
    VulkanQueue m_graphicsQueue = VK_NULL_HANDLE;
    VulkanQueue m_presentQueue = VK_NULL_HANDLE;
    
    std::mutex m_mutex;
    
    VulkanTexture* m_defaultTexture = nullptr;

    QueueFamilyIndices m_queueFamilies;

    std::vector<const char*> m_enabledDeviceExtensions;
};
#endif