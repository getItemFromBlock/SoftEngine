#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "VulkanDescriptorPool.h"
#include "VulkanDescriptorSet.h"
#include "VulkanUniformBuffer.h"

class VulkanPipeline;
class VulkanDevice;
class Texture;

class VulkanMaterial
{
public:
    VulkanMaterial(VulkanPipeline* pipeline);
    ~VulkanMaterial();

    bool Initialize(uint32_t maxFramesInFlight, Texture* defaultTexture, VulkanPipeline* pipeline);
    void Cleanup();

    void SetUniformData(uint32_t set, uint32_t binding, const void* data, size_t size, VulkanRenderer* renderer);
    void SetTexture(uint32_t set, uint32_t binding, Texture* texture, VulkanRenderer* renderer);
    void SetTextureForFrame(uint32_t frameIndex, uint32_t set, uint32_t binding, Texture* texture);
    
    void Bind(VulkanRenderer* renderer);
    void BindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t frameIndex);

    VulkanUniformBuffer* GetUniformBuffer(uint32_t set, uint32_t binding) const;
    VulkanDescriptorSet* GetDescriptorSet(uint32_t set) const;
    void SetStorageBuffer(uint32_t set, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range,
                          VulkanRenderer* renderer);
    void BindForCompute(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void DispatchCompute(VulkanRenderer* renderer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void SetPushConstants(VulkanRenderer* renderer, const void* data, uint32_t size, uint32_t offset);
    void SetStorageBufferData(uint32_t set, uint32_t binding, const void* data, size_t size, VulkanRenderer* renderer);
    VulkanPipeline* GetPipeline() const { return m_pipeline; }

private:
    VulkanPipeline* m_pipeline = nullptr;
    VulkanDevice* m_device = nullptr;
    uint32_t m_maxFramesInFlight = 0;

    std::unique_ptr<VulkanDescriptorPool> m_descriptorPool;
    std::unordered_map<UBOBinding, std::unique_ptr<VulkanUniformBuffer>> m_uniformBuffers;
    std::vector<std::unique_ptr<VulkanDescriptorSet>> m_descriptorSets;
    std::unordered_map<uint32_t, std::vector<Uniform>> m_uniformsBySet;
};