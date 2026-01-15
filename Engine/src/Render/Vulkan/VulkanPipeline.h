#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

#include "VulkanDevice.h"
#include "VulkanDescriptorSetLayout.h"
#include "VulkanUniformBuffer.h"

struct Uniform;
class Shader;
class Texture;
class VertexShader;
class FragmentShader;
class ComputeShader;
class VulkanMaterial;

class VulkanPipeline
{
public:
    VulkanPipeline() = default;
    ~VulkanPipeline();

    bool Initialize(VulkanDevice* device, VkExtent2D extent,
                    uint32_t maxFramesInFlight, const Shader* shader,
                    VkFormat colorFormat, VkFormat depthFormat);

    bool InitializeGraphicsPipeline(const Shader* shader,
                                    const VertexShader* vertexShader,
                                    const FragmentShader* fragmentShader,
                                    VkFormat colorFormat, VkFormat depthFormat);

    bool InitializeComputePipeline(const ComputeShader* computeShader);

    void Cleanup();
    void Bind(VkCommandBuffer commandBuffer, VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS);

    // Getters
    VkPipeline GetPipeline() const { return m_pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return m_pipelineLayout; }
    VulkanDevice* GetDevice() const { return m_device; }
    uint32_t GetMaxFramesInFlight() const { return m_maxFramesInFlight; }

    std::vector<VulkanDescriptorSetLayout*> GetDescriptorSetLayouts() const
    {
        std::vector<VulkanDescriptorSetLayout*> layouts;
        layouts.reserve(m_descriptorSetLayouts.size());
        for (auto& layout : m_descriptorSetLayouts)
            layouts.push_back(layout.get());
        return layouts;
    }

    const std::unordered_map<uint32_t, std::vector<Uniform>>& GetUniformsBySet() const
    {
        return m_uniformsBySet;
    }

    const std::unordered_map<VkDescriptorType, uint32_t>& GetDescriptorTypeCounts() const
    {
        return m_descriptorTypeCounts;
    }

private:
    VkShaderModule CreateShaderModule(const std::vector<char>& code) const;
    static std::vector<char> ReadFileBin(const std::string& filename);
    static std::string ReadFile(const std::string& filename);

    VulkanDevice* m_device = nullptr;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;

    uint32_t m_maxFramesInFlight = 0;

    std::vector<std::unique_ptr<VulkanDescriptorSetLayout>> m_descriptorSetLayouts;

    std::unordered_map<uint32_t, std::vector<Uniform>> m_uniformsBySet;
    std::unordered_map<UBOBinding, uint32_t> m_uniformBufferSizes;
    std::unordered_map<VkDescriptorType, uint32_t> m_descriptorTypeCounts;
};
