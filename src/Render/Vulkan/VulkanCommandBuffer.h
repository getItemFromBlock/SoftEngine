#pragma once
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <vector>

class VulkanDevice;

class VulkanCommandBuffer
{
public:
    VulkanCommandBuffer() = default;
    ~VulkanCommandBuffer() = default;

    bool Initialize(VulkanDevice* device, uint32_t imageCount);
    void Cleanup();

    VkCommandPool GetCommandPool() const { return m_commandPool; }
    const std::vector<VkCommandBuffer>& GetCommandBuffers() const { return m_commandBuffers; }
    VkCommandBuffer GetCommandBuffer(size_t index) const { return m_commandBuffers[index]; }

    void BeginRecording(size_t index);
    void EndRecording(size_t index);
    void Reset(size_t index);

private:
    VulkanDevice* m_device = nullptr;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
};

#endif