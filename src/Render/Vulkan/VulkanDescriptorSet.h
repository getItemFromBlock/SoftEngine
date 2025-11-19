#pragma once

#ifdef RENDER_API_VULKAN
#include <vulkan/vulkan.h>
#include <vector>

class VulkanDevice;
class VulkanUniformBuffer;
class VulkanTexture;

class VulkanDescriptorSet
{
public:
    VulkanDescriptorSet() = default;
    ~VulkanDescriptorSet();

    bool Initialize(VulkanDevice* device, VkDescriptorPool pool, 
                   VkDescriptorSetLayout layout, uint32_t count);
    void Cleanup();

    // Update descriptor set with uniform buffer and texture
    void UpdateDescriptorSet(uint32_t index, VulkanUniformBuffer* uniformBuffer, 
                            VulkanTexture* texture);

    VkDescriptorSet GetDescriptorSet(uint32_t index) const;

private:
    VulkanDevice* m_device = nullptr;
    std::vector<VkDescriptorSet> m_descriptorSets;
};
#endif