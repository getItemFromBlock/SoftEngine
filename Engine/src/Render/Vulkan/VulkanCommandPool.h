#pragma once
#ifdef RENDER_API_VULKAN
#include "EngineAPI.h"

#include <mutex>
#include <vulkan/vulkan.h>
#include <vector>

class VulkanDevice;

class ENGINE_API VulkanCommandPool
{
public:
    VulkanCommandPool() = default;
    ~VulkanCommandPool();

    bool Initialize(VulkanDevice* device, uint32_t imageCount);
    void Cleanup();

    VkCommandPool GetCommandPool() const { return m_commandPool; }
    VkCommandPool GetTransferCommandPool() const { return m_transferCommandPool; }
    const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return m_commandBuffers; }
    VkCommandBuffer GetCommandBuffer(size_t index) const { return m_commandBuffers[index]; }

    void BeginRecording(size_t index);
    void EndRecording(size_t index);
    void Reset(size_t index);

    std::mutex& GetMutex() { return m_mutex; }
private:
    VulkanDevice* m_device = nullptr;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandPool m_transferCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    
    std::mutex m_mutex;
};

#endif