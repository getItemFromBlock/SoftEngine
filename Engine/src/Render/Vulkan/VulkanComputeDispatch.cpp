#include "VulkanComputeDispatch.h"

#include "VulkanPipeline.h"
#include "Debug/Log.h"
#include "Resource/Shader.h"

/*
bool VulkanComputeDispatch::Initialize(VulkanDevice* device, Shader* shader)
{
    auto m_maxFramesInFlight = 2;
    m_descriptorPool = std::make_unique<VulkanDescriptorPool>();
    VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    std::vector<VulkanDescriptorSetLayout*> setLayouts = pipeline->GetDescriptorSetLayouts();
    
    auto descriptorTypeCounts = pipeline->GetDescriptorTypeCounts();
    auto descriptorSetLayouts = pipeline->GetDescriptorSetLayouts();

    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.reserve(descriptorTypeCounts.size());
    for (const auto& [type, count] : descriptorTypeCounts)
    {
        poolSizes.push_back({
            .type = type,
            .descriptorCount = count * m_maxFramesInFlight
        });
    }
    
    if (!m_descriptorPool->Initialize(device, poolSizes, 
        static_cast<uint32_t>(m_maxFramesInFlight * descriptorSetLayouts.size())))
    {
        PrintError("Failed to initialize descriptor pool!");
        return false;
    }
    
    // Allocate descriptor sets
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool->GetPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayouts[setIndex]->GetLayout();

    VkDescriptorSet descriptorSet;
    vkAllocateDescriptorSets(m_device->GetDevice(), &allocInfo, &descriptorSet);

    // Update descriptor sets with your buffers
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = yourStorageBuffer;
    bufferInfo.offset = 0;
    bufferInfo.range = bufferSize;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = bindingIndex;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(m_device->GetDevice(), 1, &descriptorWrite, 0, nullptr);
}
*/