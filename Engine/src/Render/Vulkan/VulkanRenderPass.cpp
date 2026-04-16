#include "VulkanRenderPass.h"

#include "VulkanDevice.h"
#include <iostream>
#include <stdexcept>
#include <array>

#include "VulkanDepthBuffer.h"
#include "VulkanGBuffer.h"
#include "VulkanUtils.h"
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
                             const std::vector<VkClearValue>& clearValues, bool clearAttachment)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    
    colorAttachment.loadOp = clearAttachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = clearValues.empty() ? VkClearValue{{0.0f, 0.0f, 0.0f, 1.0f}} : clearValues[0];

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = clearAttachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue = clearValues.size() > 1 ? clearValues[1] : VkClearValue{1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = depthImageView ? &depthAttachment : nullptr;
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

void VulkanRenderPass::BeginGBuffer(VkCommandBuffer commandBuffer,
                                    VulkanGBuffer* gBuffer,
                                    VkImageView depthImageView,
                                    VkExtent2D extent)
{
    gBuffer->MarkUsed();

    VkClearValue clearBlack{};
    clearBlack.color = {{0.0f, 0.0f, 0.0f, 1.0f}};

    auto makeColorAttach = [&](VkImageView view) -> VkRenderingAttachmentInfo
    {
        VkRenderingAttachmentInfo a{};
        a.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        a.imageView = view;
        a.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        a.clearValue = clearBlack;
        return a;
    };

    std::array<VkRenderingAttachmentInfo, 4> colorAttachments = {
        makeColorAttach(gBuffer->GetPosition().imageView),
        makeColorAttach(gBuffer->GetNormal().imageView),
        makeColorAttach(gBuffer->GetAlbedo().imageView),
        makeColorAttach(gBuffer->GetMetallicRoughness().imageView)
    };

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = depthImageView;
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.clearValue = {1.0f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    renderingInfo.pColorAttachments = colorAttachments.data();
    renderingInfo.pDepthAttachment = &depthAttachment;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void VulkanRenderPass::EndGBuffer(VkCommandBuffer commandBuffer, VulkanGBuffer* gBuffer)
{
    vkCmdEndRendering(commandBuffer);
}

void VulkanRenderPass::BeginComposition(VkCommandBuffer commandBuffer,
                                        VkImageView colorImageView,
                                        VkExtent2D extent)
{
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorImageView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = extent;
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);
}