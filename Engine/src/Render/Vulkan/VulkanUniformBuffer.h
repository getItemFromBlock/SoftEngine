#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

class VulkanDevice;
class VulkanBuffer;

struct UBOBinding
{
    int set;
    int binding;
    
    UBOBinding(int set, int binding) : set(set), binding(binding) {}
    UBOBinding(uint32_t set, uint32_t binding) : UBOBinding(static_cast<int>(set), static_cast<int>(binding)) {}
    UBOBinding() : UBOBinding(0, 0) {}
    
    bool operator==(const UBOBinding& other) const noexcept
    {
        return set == other.set && binding == other.binding;
    }
};

namespace std
{
    template<>
    struct hash<UBOBinding>
    {
        std::size_t operator()(const UBOBinding& k) const noexcept
        {
            std::size_t h1 = std::hash<int>()(k.set);
            std::size_t h2 = std::hash<int>()(k.binding);

            std::size_t seed = h1;
            seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}

class VulkanUniformBuffer
{
public:
    VulkanUniformBuffer() = default;
    virtual ~VulkanUniformBuffer();

    bool Initialize(VulkanDevice* device, VkDeviceSize size, 
        uint32_t frameCount, VkBufferUsageFlags usageFlags = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    void Cleanup();

    void UpdateData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    void UpdateData(const void* data, VkDeviceSize size, uint32_t currentFrame, VkDeviceSize offset = 0);

    void* Map(uint32_t currentFrame);

    bool MapAll();

    void Unmap(uint32_t currentFrame);

    void UnmapAll();
    
    void WriteToMapped(const void* data, VkDeviceSize size, uint32_t currentFrame, VkDeviceSize offset = 0);

    VkBuffer GetBuffer(size_t i) const;
    VkDeviceSize GetSize() const { return m_size; }
    uint32_t GetFrameCount() const { return static_cast<uint32_t>(m_buffer.size()); }

    VkDescriptorBufferInfo GetDescriptorInfo(size_t frame) const;
    VkDescriptorBufferInfo GetDescriptorInfo(size_t frame, VkDeviceSize offset, VkDeviceSize range) const;

private:
    VulkanDevice* m_device = nullptr;
    std::vector<VulkanBuffer*> m_buffer;     
    std::vector<void*> m_mappedMemory;       
    VkDeviceSize m_size = 0;
};

using UniformBuffersOwner = std::unordered_map<UBOBinding, std::unique_ptr<VulkanUniformBuffer>>;
using UniformBuffers = std::unordered_map<UBOBinding, VulkanUniformBuffer*>;