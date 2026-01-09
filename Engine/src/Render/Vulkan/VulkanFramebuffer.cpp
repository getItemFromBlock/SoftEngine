#include "VulkanFramebuffer.h"

#include <array>

#include "VulkanDevice.h"
#include <iostream>
#include <stdexcept>
#include <string>

#include "VulkanDepthBuffer.h"
#include "Debug/Log.h"

VulkanFramebuffer::~VulkanFramebuffer()
{
    Cleanup();
}

bool VulkanFramebuffer::Initialize(VulkanDevice* device, VkRenderPass renderPass,
                                   const std::vector<VkImageView>& imageViews, VkExtent2D extent, VulkanDepthBuffer* depthBuffer)
{
    if (!device || renderPass == VK_NULL_HANDLE || imageViews.empty())
    {
        std::cerr << "Invalid parameters for framebuffer initialization!" << std::endl;
        return false;
    }

    m_device = device;
    m_framebuffers.clear();
    m_framebuffers.resize(imageViews.size());

    try
    {
        for (size_t i = 0; i < imageViews.size(); i++)
        {
            std::array<VkImageView, 2> attachments = {
                imageViews[i],
                depthBuffer->GetImageView()
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            VkResult result = vkCreateFramebuffer(m_device->GetDevice(), &framebufferInfo, nullptr, &m_framebuffers[i]);
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create framebuffer " + std::to_string(i) + 
                                         "! Error code: " + std::to_string(result));
            }
        }

        PrintLog("Created %d framebuffers", m_framebuffers.size());
        return true;
    }
    catch (const std::exception& e)
    {
        PrintError("Framebuffer initialization failed: %s", e.what());
        Cleanup();
        return false;
    }
}

void VulkanFramebuffer::Cleanup()
{
    if (m_device)
    {
        for (auto framebuffer : m_framebuffers)
        {
            if (framebuffer != VK_NULL_HANDLE)
            {
                vkDestroyFramebuffer(m_device->GetDevice(), framebuffer, nullptr);
            }
        }
    }
    m_framebuffers.clear();
}
