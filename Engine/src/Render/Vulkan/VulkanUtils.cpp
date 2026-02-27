#include "VulkanUtils.h"

#include <array>

#include "VulkanGBuffer.h"

void VulkanUtils::TransitionImageLayout(VulkanCommandPool *_commandBuffer, VulkanQueue &graphicsQueue, VkImageLayout oldLayout,
                                        VkImageLayout newLayout, VulkanDevice * m_device, VkImage image, uint32_t layerCount)
{
    VkCommandBuffer commandBuffer = m_device->BeginSingleTimeCommands(_commandBuffer);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        //barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    else
    {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && 
        newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        /*
        if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
        {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        */
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else
    {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    m_device->EndSingleTimeCommands(_commandBuffer, graphicsQueue, commandBuffer);
}

void VulkanUtils::TransitionGBufferToColorAttachment(VkCommandBuffer commandBuffer, VulkanGBuffer& gBuffer)
{
    VkImageLayout srcLayout = gBuffer.IsFirstUse()
                                  ? VK_IMAGE_LAYOUT_UNDEFINED
                                  : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkImageSubresourceRange range{};
    range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel   = 0;
    range.levelCount     = 1;
    range.baseArrayLayer = 0;
    range.layerCount     = 1;

    auto makeBarrier = [&](VkImage image) -> VkImageMemoryBarrier
    {
        VkImageMemoryBarrier b{};
        b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout           = srcLayout;
        b.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image               = image;
        b.subresourceRange    = range;
        b.srcAccessMask       = gBuffer.IsFirstUse() ? 0 : VK_ACCESS_SHADER_READ_BIT;
        b.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        return b;
    };

    std::array<VkImageMemoryBarrier, 3> barriers = {
        makeBarrier(gBuffer.GetPosition().image),
        makeBarrier(gBuffer.GetNormal().image),
        makeBarrier(gBuffer.GetAlbedo().image),
    };

    vkCmdPipelineBarrier(commandBuffer,
                         gBuffer.IsFirstUse() ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                             : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0,
                         0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(barriers.size()), barriers.data());
}

void VulkanUtils::TransitionGBufferToShaderRead(VkCommandBuffer commandBuffer, VulkanGBuffer& gBuffer)
{
    VkImageSubresourceRange range{};
    range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel   = 0;
    range.levelCount     = 1;
    range.baseArrayLayer = 0;
    range.layerCount     = 1;

    auto makeBarrier = [&](VkImage image) -> VkImageMemoryBarrier
    {
        VkImageMemoryBarrier b{};
        b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image               = image;
        b.subresourceRange    = range;
        b.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        b.dstAccessMask       = VK_ACCESS_SHADER_READ_BIT;
        return b;
    };

    std::array<VkImageMemoryBarrier, 3> barriers = {
        makeBarrier(gBuffer.GetPosition().image),
        makeBarrier(gBuffer.GetNormal().image),
        makeBarrier(gBuffer.GetAlbedo().image),
    };

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0,
                         0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(barriers.size()), barriers.data());
}
