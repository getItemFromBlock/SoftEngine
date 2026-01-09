#include "VulkanRenderPass.h"

#include "VulkanDevice.h"
#include <iostream>
#include <stdexcept>
#include <array>

#include "VulkanDepthBuffer.h"
#include "Debug/Log.h"

VulkanRenderPass::~VulkanRenderPass()
{
    Cleanup();
}

bool VulkanRenderPass::Initialize(VulkanDevice* device, VkFormat swapChainImageFormat)
{
    m_swapChainImageFormat = swapChainImageFormat;
    m_depthFormat = VulkanDepthBuffer::FindDepthFormat(device);
    
    if (!device)
    {
        std::cerr << "Invalid device for render pass initialization!" << std::endl;
        return false;
    }

    m_device = device;
    return true;
}

void VulkanRenderPass::Cleanup()
{
    
}

void VulkanRenderPass::Begin(VkCommandBuffer commandBuffer, VkImageView colorImageView, 
                             VkImageView depthImageView, VkExtent2D extent, 
                             const std::vector<VkClearValue>& clearValues)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = clearValues.empty() ? VkClearValue{{0.0f, 0.0f, 0.0f, 1.0f}} : clearValues[0];

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue = clearValues.size() > 1 ? clearValues[1] : VkClearValue{1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void VulkanRenderPass::End(VkCommandBuffer commandBuffer)
{
    vkCmdEndRendering(commandBuffer);
}

VkFormat VulkanRenderPass::GetColorFormat() const
{
    return m_swapChainImageFormat;
}

VkFormat VulkanRenderPass::GetDepthFormat() const
{
    return m_depthFormat;
}
