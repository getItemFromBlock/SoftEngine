#pragma once
#include "Resource/FragmentShader.h"
#include "Resource/VertexShader.h"
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

#include "VulkanUniformBuffer.h"
#include "VulkanDescriptorPool.h"
#include "VulkanDescriptorSet.h"
#include "VulkanDescriptorSetLayout.h"
#include "Render/RHI/RHIPipeline.h"

struct Uniform;
class VulkanDevice;

class VulkanPipeline : public RHIPipeline
{
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    bool Initialize(VulkanDevice* device,
                    VkRenderPass renderPass,
                    VkExtent2D extent,
                    const std::string& vertShaderPath,
                    const std::string& fragShaderPath,
                    const std::vector<VkDescriptorSetLayoutBinding>& bindings = {},
                    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT,
                    bool enableDepth = true, bool compiled = false);
    
    bool Initialize(VulkanDevice* device,
                    VkRenderPass renderPass,
                    VkExtent2D extent,
                    const Uniforms& uniforms, const VertexShader* vertexShader, const FragmentShader* fragShader, uint32_t
                    MAX_FRAMES_IN_FLIGHT, Texture* defaultTexture);
    
    void Cleanup();

    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
    std::vector<VulkanDescriptorSetLayout*> GetDescriptorSetLayouts() const;
    std::vector<VulkanDescriptorSet*> GetDescriptorSets() const;

    void Bind(VkCommandBuffer commandBuffer);

    UniformBuffers GetUniformBuffers() const;
    VulkanUniformBuffer* GetUniformBuffer(UBOBinding binding) const;
    VulkanUniformBuffer* GetUniformBuffer(int set, int binding) const;

private:
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
    std::vector<char> ReadFileBin(const std::string& filename);
    std::string ReadFile(const std::string& filename);

    VulkanDevice* m_device = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    
    std::vector<std::unique_ptr<VulkanDescriptorSetLayout>> m_descriptorSetLayouts;
    std::unique_ptr<VulkanDescriptorPool> m_descriptorPool;
    std::vector<std::unique_ptr<VulkanDescriptorSet>> m_descriptorSets;
    UniformBuffersOwner m_uniformBuffers;
};

#endif