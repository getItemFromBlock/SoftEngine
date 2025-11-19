#pragma once
#ifdef RENDER_API_VULKAN

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class VulkanDevice;

class VulkanPipeline
{
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    bool Initialize(VulkanDevice* device,
                    VkRenderPass renderPass,
                    VkExtent2D extent,
                    const std::string& vertShaderPath,
                    const std::string& fragShaderPath,
                    const std::vector<VkDescriptorSetLayout>& setLayouts = {},
                    const std::vector<VkDescriptorSetLayoutBinding>& bindings = {},
                    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT, bool enableDepth = true);
    void Cleanup();

    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
    VkDescriptorSetLayout GetDescriptorSetLayout() const { return m_descriptorSetLayout; }
    
    void Bind(VkCommandBuffer commandBuffer);

private:
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
    std::vector<char> ReadFile(const std::string& filename);

    VulkanDevice* m_device = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
};

#endif