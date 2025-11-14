#include "VulkanFramebuffer.h"
#ifdef RENDER_API_VULKAN

#include "VulkanDevice.h"
#include <iostream>
#include <stdexcept>
#include <string>

bool VulkanFramebuffer::Initialize(VulkanDevice* device, VkRenderPass renderPass,
                                   const std::vector<VkImageView>& imageViews, VkExtent2D extent)
{
    if (!device || renderPass == VK_NULL_HANDLE || imageViews.empty())
    {
        std::cerr << "Invalid parameters for framebuffer initialization!" << std::endl;
        return false;
    }

    m_device = device;
    m_framebuffers.resize(imageViews.size());

    try
    {
        for (size_t i = 0; i < imageViews.size(); i++)
        {
            VkImageView attachments[] = { imageViews[i] };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1;

            VkResult result = vkCreateFramebuffer(m_device->GetDevice(), &framebufferInfo, 
                                                  nullptr, &m_framebuffers[i]);
            if (result != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create framebuffer " + std::to_string(i) + 
                                         "! Error code: " + std::to_string(result));
            }
        }

        std::cout << "Created " << m_framebuffers.size() << " framebuffers" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Framebuffer initialization failed: " << e.what() << std::endl;
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

#endif