#pragma once
#include "Resource/Shader.h"

#include <vulkan/vulkan.h>
#include <vector>

#include "VulkanUniformBuffer.h"

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

    VkDescriptorSet GetDescriptorSet(uint32_t index) const;
    
    void UpdateDescriptorSets(uint32_t frameIndex, uint32_t index, const Uniforms& uniforms, const UniformBuffers& uniformBuffers, Texture* defaultTexture) const;

private:
    VulkanDevice* m_device = nullptr;
    std::vector<VkDescriptorSet> m_descriptorSets;
    VkDescriptorPool m_pool;
};