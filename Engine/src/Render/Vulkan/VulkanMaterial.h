#pragma once
#ifdef RENDER_API_VULKAN

#include <memory>
#include <unordered_map>
#include <vector>

#include "VulkanDescriptorPool.h"
#include "VulkanDescriptorSet.h"
#include "VulkanUniformBuffer.h"
#include "Render/RHI/RHIMaterial.h"

class VulkanPipeline;
class VulkanDevice;
class Texture;

class VulkanMaterial : public RHIMaterial
{
public:
    VulkanMaterial(VulkanPipeline* pipeline);
    ~VulkanMaterial() override;

    bool Initialize(uint32_t maxFramesInFlight, Texture* defaultTexture, VulkanPipeline* pipeline);
    void Cleanup() override;

    void SetUniformData(uint32_t set, uint32_t binding, const void* data, size_t size, RHIRenderer* renderer) override;
    void SetTexture(uint32_t set, uint32_t binding, Texture* texture, RHIRenderer* renderer);
    void SetTextureForFrame(uint32_t frameIndex, uint32_t set, uint32_t binding, Texture* texture);
    
    void Bind(RHIRenderer* renderer) override;
    void BindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t frameIndex);

    VulkanUniformBuffer* GetUniformBuffer(uint32_t set, uint32_t binding) const;
    VulkanDescriptorSet* GetDescriptorSet(uint32_t set) const;
    void SetStorageBuffer(uint32_t set, uint32_t binding, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range,
                          RHIRenderer* renderer);
    void BindForCompute(VkCommandBuffer commandBuffer, uint32_t frameIndex);
    void DispatchCompute(RHIRenderer* renderer, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);
    void SetPushConstants(RHIRenderer* renderer, const void* data, uint32_t size, uint32_t offset);
    void SetStorageBufferData(uint32_t set, uint32_t binding, const void* data, size_t size, RHIRenderer* renderer);
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

#endif