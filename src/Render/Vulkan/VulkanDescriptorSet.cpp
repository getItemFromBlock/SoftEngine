#include "VulkanDescriptorSet.h"
#include "VulkanDevice.h"
#include "VulkanUniformBuffer.h"
#include "VulkanTexture.h"
#include <stdexcept>
#include <iostream>

VulkanDescriptorSet::~VulkanDescriptorSet()
{
    Cleanup();
}

bool VulkanDescriptorSet::Initialize(VulkanDevice* device, VkDescriptorPool pool,
                                     VkDescriptorSetLayout layout, uint32_t count)
{
    m_device = device;
    m_descriptorSets.resize(count);

    std::vector<VkDescriptorSetLayout> layouts(count, layout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts.data();

    if (vkAllocateDescriptorSets(m_device->GetDevice(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate descriptor sets!" << std::endl;
        return false;
    }

    std::cout << "Descriptor sets allocated successfully! Count: " << count << std::endl;
    return true;
}

void VulkanDescriptorSet::Cleanup()
{
    // Note: Descriptor sets are automatically freed when the pool is destroyed
    // No need to explicitly free them
    m_descriptorSets.clear();
}

void VulkanDescriptorSet::UpdateDescriptorSet(uint32_t index, VulkanUniformBuffer* uniformBuffer,
                                              VulkanTexture* texture)
{
    if (index >= m_descriptorSets.size())
    {
        throw std::runtime_error("Invalid descriptor set index!");
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites;

    // Update uniform buffer descriptor
    for (size_t i = 0; i < uniformBuffer->GetFrameCount(); i++)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffer->GetBuffer(i);
        bufferInfo.offset = 0;
        bufferInfo.range = uniformBuffer->GetSize();

        VkWriteDescriptorSet uniformWrite{};
        uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniformWrite.dstSet = m_descriptorSets[index];
        uniformWrite.dstBinding = 0;
        uniformWrite.dstArrayElement = 0;
        uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformWrite.descriptorCount = 1;
        uniformWrite.pBufferInfo = &bufferInfo;

        descriptorWrites.push_back(uniformWrite);
    }

    // Update texture sampler descriptor
    if (texture)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texture->GetImageView();
        imageInfo.sampler = texture->GetSampler();

        VkWriteDescriptorSet samplerWrite{};
        samplerWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        samplerWrite.dstSet = m_descriptorSets[index];
        samplerWrite.dstBinding = 1;
        samplerWrite.dstArrayElement = 0;
        samplerWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerWrite.descriptorCount = 1;
        samplerWrite.pImageInfo = &imageInfo;

        descriptorWrites.push_back(samplerWrite);
    }

    if (!descriptorWrites.empty())
    {
        vkUpdateDescriptorSets(m_device->GetDevice(), 
                              static_cast<uint32_t>(descriptorWrites.size()),
                              descriptorWrites.data(), 0, nullptr);
    }
}

VkDescriptorSet VulkanDescriptorSet::GetDescriptorSet(uint32_t index) const
{
    if (index >= m_descriptorSets.size())
    {
        throw std::runtime_error("Invalid descriptor set index!");
    }
    return m_descriptorSets[index];
}