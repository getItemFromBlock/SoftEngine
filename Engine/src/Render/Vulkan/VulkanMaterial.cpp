#pragma once
#ifdef RENDER_API_VULKAN

#include "VulkanMaterial.h"

#include <ranges>

#include "VulkanPipeline.h"
#include "VulkanDevice.h"
#include "VulkanUniformBuffer.h"
#include "VulkanDescriptorSet.h"
#include "VulkanDescriptorPool.h"
#include "VulkanRenderer.h"
#include "VulkanTexture.h"
#include "Resource/Texture.h"
#include "Debug/Log.h"

VulkanMaterial::VulkanMaterial(VulkanPipeline* pipeline)
    : m_pipeline(pipeline)
      , m_device(pipeline->GetDevice())
      , m_maxFramesInFlight(pipeline->GetMaxFramesInFlight())
{
    m_uniformsBySet = pipeline->GetUniformsBySet();
}

VulkanMaterial::~VulkanMaterial()
{
    VulkanMaterial::Cleanup();
}

bool VulkanMaterial::Initialize(uint32_t maxFramesInFlight, Texture* defaultTexture, VulkanPipeline* pipeline)
{
    try
    {
        // Ensure stored frame count matches requested
        m_maxFramesInFlight = maxFramesInFlight;

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

        m_descriptorPool = std::make_unique<VulkanDescriptorPool>();
        if (!m_descriptorPool->Initialize(m_device, poolSizes,
                                          static_cast<uint32_t>(m_maxFramesInFlight * descriptorSetLayouts.size())))
        {
            PrintError("Failed to initialize shared descriptor pool!");
            Cleanup();
            return false;
        }

        // --- Create Uniform Buffers (including Storage Buffers) ---
        for (const auto& [set, uniforms] : m_uniformsBySet)
        {
            for (const auto& uniform : uniforms)
            {
                // Create buffers for both NestedStruct (UBO) and StorageBuffer (SSBO)
                if (uniform.type == UniformType::NestedStruct || 
                    uniform.type == UniformType::StorageBuffer)
                {
                    UBOBinding key = {uniform.set, uniform.binding};

                    auto ubo = std::make_unique<VulkanUniformBuffer>();
                    auto type = uniform.type == UniformType::StorageBuffer ? VK_BUFFER_USAGE_STORAGE_BUFFER_BIT : VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
                    if (!ubo->Initialize(m_device, uniform.size, m_maxFramesInFlight, type))
                    {
                        PrintError("Failed to initialize uniform buffer for set %u binding %u",
                                   uniform.set, uniform.binding);
                        Cleanup();
                        return false;
                    }
                    ubo->MapAll();

                    m_uniformBuffers[key] = std::move(ubo);
                }
            }
        }

        // --- Allocate Descriptor Sets ---
        const auto& layouts = m_pipeline->GetDescriptorSetLayouts();
        m_descriptorSets.reserve(layouts.size());

        for (size_t i = 0; i < layouts.size(); ++i)
        {
            auto descriptorSet = std::make_unique<VulkanDescriptorSet>();
            if (!descriptorSet->Initialize(m_device,
                                           m_descriptorPool->GetPool(),
                                           layouts[i]->GetLayout(),
                                           m_maxFramesInFlight))
            {
                PrintError("Failed to initialize descriptor set %zu", i);
                Cleanup();
                return false;
            }
            m_descriptorSets.push_back(std::move(descriptorSet));
        }

        // --- Update Descriptor Sets with Default Values ---
        for (uint32_t frameIdx = 0; frameIdx < m_maxFramesInFlight; ++frameIdx)
        {
            for (const auto& [set, uniforms] : m_uniformsBySet)
            {
                // Reserve to avoid reallocation (stable addresses for p*Info pointers)
                size_t uniformCount = uniforms.size();
                std::vector<VkWriteDescriptorSet> writes;
                writes.reserve(uniformCount);
                std::vector<VkDescriptorBufferInfo> bufferInfos;
                std::vector<VkDescriptorImageInfo> imageInfos;
                bufferInfos.reserve(uniformCount);
                imageInfos.reserve(uniformCount);

                for (const auto& uniform : uniforms)
                {
                    VkWriteDescriptorSet write{};
                    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write.dstSet = m_descriptorSets[set]->GetDescriptorSet(frameIdx);
                    write.dstBinding = uniform.binding;
                    write.dstArrayElement = 0;
                    write.descriptorCount = 1;

                    if (uniform.type == UniformType::NestedStruct)
                    {
                        // Uniform buffer
                        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

                        UBOBinding key = {uniform.set, uniform.binding};
                        auto* ubo = m_uniformBuffers[key].get();
                        if (!ubo)
                        {
                            PrintError("Uniform buffer missing for set %u binding %u", uniform.set, uniform.binding);
                            Cleanup();
                            return false;
                        }

                        bufferInfos.emplace_back();
                        VkDescriptorBufferInfo& bufferInfo = bufferInfos.back();
                        bufferInfo.buffer = ubo->GetBuffer(frameIdx);
                        bufferInfo.offset = 0;
                        bufferInfo.range = uniform.size;

                        write.pBufferInfo = &bufferInfo;
                    }
                    else if (uniform.type == UniformType::Sampler2D ||
                             uniform.type == UniformType::SamplerCube)
                    {
                        // Texture sampler
                        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

                        imageInfos.emplace_back();
                        VkDescriptorImageInfo& imageInfo = imageInfos.back();
                        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                        // Use default texture initially
                        if (defaultTexture)
                        {
                            auto* vulkanTexture = dynamic_cast<VulkanTexture*>(defaultTexture->GetBuffer());
                            imageInfo.imageView = vulkanTexture->GetImageView();
                            imageInfo.sampler = vulkanTexture->GetSampler();
                        }
                        else
                        {
                            imageInfo.imageView = VK_NULL_HANDLE;
                            imageInfo.sampler = VK_NULL_HANDLE;
                        }

                        write.pImageInfo = &imageInfo;
                    }
                    else if (uniform.type == UniformType::StorageBuffer)
                    {
                        // Storage buffer
                        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

                        UBOBinding key = {uniform.set, uniform.binding};
                        auto* ubo = m_uniformBuffers[key].get();
                        if (!ubo)
                        {
                            PrintError("Storage buffer missing for set %u binding %u", uniform.set, uniform.binding);
                            Cleanup();
                            return false;
                        }

                        bufferInfos.emplace_back();
                        VkDescriptorBufferInfo& bufferInfo = bufferInfos.back();
                        bufferInfo.buffer = ubo->GetBuffer(frameIdx);
                        bufferInfo.offset = 0;
                        bufferInfo.range = uniform.size;

                        write.pBufferInfo = &bufferInfo;
                    }

                    writes.push_back(write);
                }

                // Perform batch update
                if (!writes.empty())
                {
                    vkUpdateDescriptorSets(m_device->GetDevice(),
                                          static_cast<uint32_t>(writes.size()),
                                          writes.data(),
                                          0, nullptr);
                }
            }
        }

        return true;
    }
    catch (const std::exception& e)
    {
        PrintError("VulkanMaterial initialization failed: %s", e.what());
        Cleanup();
        return false;
    }
}

void VulkanMaterial::Cleanup()
{
    for (auto& descriptor : m_descriptorSets)
    {
        if (descriptor)
        {
            descriptor->Cleanup();
        }
    }
    m_descriptorSets.clear();

    if (m_descriptorPool)
    {
        m_descriptorPool->Cleanup();
        m_descriptorPool.reset();
    }

    for (auto& uniformBuffer : m_uniformBuffers | std::views::values)
    {
        if (uniformBuffer)
        {
            uniformBuffer->Cleanup();
        }
    }
    m_uniformBuffers.clear();
}

void VulkanMaterial::SetUniformData(uint32_t set, uint32_t binding, const void* data,
                                    size_t size, RHIRenderer* renderer)
{
    uint32_t frameIndex = dynamic_cast<VulkanRenderer*>(renderer)->GetFrameIndex();
    UBOBinding key = {set, binding};
    auto it = m_uniformBuffers.find(key);

    if (it != m_uniformBuffers.end())
    {
        it->second->UpdateData(data, size, frameIndex);
    }
    else
    {
        PrintError("Uniform buffer not found for set %u binding %u", set, binding);
    }
}

void VulkanMaterial::SetTexture(uint32_t set, uint32_t binding, Texture* texture, RHIRenderer* renderer)
{
    VulkanRenderer* vulkanRenderer = dynamic_cast<VulkanRenderer*>(renderer);
    
    for (uint32_t frameIndex = 0; frameIndex < vulkanRenderer->GetMaxFramesInFlight(); ++frameIndex)
    {
        if (!texture || set >= m_descriptorSets.size())
        {
            PrintError("Invalid texture or set index");
            return;
        }

        auto* vulkanTexture = dynamic_cast<VulkanTexture*>(texture->GetBuffer());
        
        if (!vulkanTexture)
        {
            PrintError("Invalid texture");
            return;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = vulkanTexture->GetImageView();
        imageInfo.sampler = vulkanTexture->GetSampler();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_descriptorSets[set]->GetDescriptorSet(frameIndex);
        write.dstBinding = binding;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_device->GetDevice(), 1, &write, 0, nullptr);
    }
}

void VulkanMaterial::Bind(RHIRenderer* renderer)
{
    VulkanRenderer* vulkanRenderer = dynamic_cast<VulkanRenderer*>(renderer);
    BindDescriptorSets(vulkanRenderer->GetCommandBuffer(), vulkanRenderer->GetFrameIndex());
}

void VulkanMaterial::BindDescriptorSets(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
    std::vector<VkDescriptorSet> sets;
    sets.reserve(m_descriptorSets.size());

    for (const auto& descriptorSet : m_descriptorSets)
    {
        sets.push_back(descriptorSet->GetDescriptorSet(frameIndex));
    }

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline->GetPipelineLayout(),
                            0,
                            static_cast<uint32_t>(sets.size()),
                            sets.data(),
                            0, nullptr);
}

VulkanUniformBuffer* VulkanMaterial::GetUniformBuffer(uint32_t set, uint32_t binding) const
{
    UBOBinding key = {set, binding};
    auto it = m_uniformBuffers.find(key);
    return (it != m_uniformBuffers.end()) ? it->second.get() : nullptr;
}

VulkanDescriptorSet* VulkanMaterial::GetDescriptorSet(uint32_t set) const
{
    return (set < m_descriptorSets.size()) ? m_descriptorSets[set].get() : nullptr;
}

void VulkanMaterial::SetStorageBuffer(
    uint32_t set, uint32_t binding,
    VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range,
    RHIRenderer* renderer)
{
    VulkanRenderer* vkRenderer = Cast<VulkanRenderer>(renderer);
    uint32_t frameIndex = vkRenderer->GetFrameIndex();

    VkDescriptorBufferInfo info{};
    info.buffer = buffer;
    info.offset = offset;
    info.range  = range;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptorSets[set]->GetDescriptorSet(frameIndex);
    write.dstBinding = binding;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &info;

    vkUpdateDescriptorSets(m_device->GetDevice(), 1, &write, 0, nullptr);
}


void VulkanMaterial::BindForCompute(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
    std::vector<VkDescriptorSet> sets;
    sets.reserve(m_descriptorSets.size());

    for (const auto& descriptorSet : m_descriptorSets)
    {
        sets.push_back(descriptorSet->GetDescriptorSet(frameIndex));
    }

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            m_pipeline->GetPipelineLayout(),
                            0,
                            static_cast<uint32_t>(sets.size()),
                            sets.data(),
                            0,
                            nullptr);
}

void VulkanMaterial::DispatchCompute(RHIRenderer* renderer, uint32_t groupCountX, 
                                     uint32_t groupCountY, uint32_t groupCountZ)
{
    VulkanRenderer* vulkanRenderer = dynamic_cast<VulkanRenderer*>(renderer);
    VkCommandBuffer cmdBuf = vulkanRenderer->GetCommandBuffer();
    uint32_t frameIndex = vulkanRenderer->GetFrameIndex();

    m_pipeline->Bind(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE);
    BindForCompute(cmdBuf, frameIndex);
    vkCmdDispatch(cmdBuf, groupCountX, groupCountY, groupCountZ);
}

void VulkanMaterial::SetPushConstants(RHIRenderer* renderer, const void* data, 
                                      uint32_t size, uint32_t offset)
{
    VulkanRenderer* vulkanRenderer = dynamic_cast<VulkanRenderer*>(renderer);
    VkCommandBuffer cmdBuf = vulkanRenderer->GetCommandBuffer();

    // Note: You may want to make shader stages configurable
    // For now, using COMPUTE_BIT as this is in compute shader context
    vkCmdPushConstants(cmdBuf,
                       m_pipeline->GetPipelineLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       offset,
                       size,
                       data);
}

void VulkanMaterial::SetStorageBufferData(uint32_t set, uint32_t binding, const void* data,
                                          size_t size, RHIRenderer* renderer)
{
    uint32_t frameIndex = dynamic_cast<VulkanRenderer*>(renderer)->GetFrameIndex();
    UBOBinding key = {set, binding};
    auto it = m_uniformBuffers.find(key);

    if (it != m_uniformBuffers.end())
    {
        it->second->UpdateData(data, size, frameIndex);
    }
    else
    {
        PrintError("Storage buffer not found for set %u binding %u", set, binding);
    }
}

#endif