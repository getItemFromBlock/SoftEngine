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
    ~VulkanPipeline() = default;

    bool Initialize(VulkanDevice* device, VkRenderPass renderPass, VkExtent2D extent,
                   const std::string& vertShaderPath, const std::string& fragShaderPath);
    void Cleanup();

    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }

    void Bind(VkCommandBuffer commandBuffer);

private:
    VkShaderModule CreateShaderModule(const std::vector<char>& code);
    std::vector<char> ReadFile(const std::string& filename);

    VulkanDevice* m_device = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
};

#endif