#include "VulkanDescriptorSet.h"
#include "VulkanDevice.h"
#include "VulkanUniformBuffer.h"
#include "VulkanTexture.h"
#include <stdexcept>
#include <iostream>
#include <ranges>

#include "Debug/Log.h"
#include "Resource/Texture.h"

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
        PrintError("Failed to allocate descriptor sets!");
        return false;
    }

    PrintLog("Descriptor sets allocated successfully! Count: %d", count);
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
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = uniformBuffer->GetBuffer(index);
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

    // Update texture sampler descriptor (if provided)
    VkDescriptorImageInfo imageInfo{};
    if (!texture)
    {
        texture = m_device->GetDefaultTexture();
    }
    if (texture)
    {
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

void VulkanDescriptorSet::UpdateDescriptorSets(uint32_t frameIndex, uint32_t index, const Uniforms& uniforms, const UniformBuffers& uniformBuffers, Texture* defaultTexture) const
{
    size_t bufferCount = 0, imageCount = 0;
    for (const auto& uniform : uniforms | std::views::values)
    {
        if (uniform.set == index)
        {
            if (uniform.type == UniformType::NestedStruct) 
                ++bufferCount;
            else if (uniform.type == UniformType::Sampler2D) 
                ++imageCount;
        }
    }

    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorBufferInfo> descBufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;
    descriptorWrites.reserve(bufferCount + imageCount);
    descBufferInfos.reserve(bufferCount);
    imageInfos.reserve(imageCount);

    for (const auto& uniform : uniforms | std::views::values)
    {
        if (uniform.set != index) continue;

        if (uniform.type == UniformType::NestedStruct)
        {
            auto uboIt = uniformBuffers.find({index, uniform.binding});
            
            if (uboIt == uniformBuffers.end())
            {
                PrintError("Failed to find uniform buffer for set %s binding %s!", index, uniform.binding);
                continue; 
            }
            
            VulkanUniformBuffer* ubo = dynamic_cast<VulkanUniformBuffer*>(uboIt->second);
            
            VkDescriptorBufferInfo descBufferInfo{};
            descBufferInfo.buffer = ubo->GetBuffer(frameIndex); 
            descBufferInfo.offset = 0;
            descBufferInfo.range = ubo->GetSize(); 
            descBufferInfos.push_back(descBufferInfo);
        }
        else if (uniform.type == UniformType::Sampler2D)
        {
            VulkanTexture* vulkanTexture = dynamic_cast<VulkanTexture*>(defaultTexture->GetBuffer());
            if (!vulkanTexture) 
                continue;
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = vulkanTexture->GetImageView();
            imageInfo.sampler = vulkanTexture->GetSampler();
            imageInfos.push_back(imageInfo);
        }

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.pNext = nullptr;
        descriptorWrite.dstSet = GetDescriptorSet(frameIndex);
        descriptorWrite.dstBinding = uniform.binding;
        descriptorWrite.dstArrayElement = 0;

        if (uniform.type == UniformType::NestedStruct)
        {
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &descBufferInfos.back();
            descriptorWrite.pImageInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;
        }
        else if (uniform.type == UniformType::Sampler2D)
        {
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfos.back();
            descriptorWrite.pBufferInfo = nullptr;
            descriptorWrite.pTexelBufferView = nullptr;
        }
        else
        {
            continue;
        }

        descriptorWrites.push_back(descriptorWrite);
    }

    if (!descriptorWrites.empty())
    {
        vkUpdateDescriptorSets(m_device->GetDevice(),
                               static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}
