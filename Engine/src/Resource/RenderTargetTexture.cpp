#include "RenderTargetTexture.h"

#include "Debug/Log.h"
#include "Render/Vulkan/VulkanRenderer.h"

bool RenderTargetTexture::Load(ResourceManager* resourceManager)
{
    return true;
}

bool RenderTargetTexture::SendToGPU(VulkanRenderer* renderer)
{
    return true;
}

void RenderTargetTexture::Unload()
{
    Texture::Unload();
    
    if (m_depthBuffer)
    {
        m_depthBuffer->Cleanup();
        m_depthBuffer.reset();
    }
}

void RenderTargetTexture::CreateRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height, VkFilter filter)
{
    VulkanDevice* device = renderer->GetDevice();
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = renderer->GetSwapChain()->GetImageFormat();
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    m_buffer = std::make_unique<VulkanTexture>();
    m_buffer->SetPreferredFilterType(filter);
    m_buffer->CreateRenderTarget(imageInfo, device, renderer->GetCommandPool(), 
                                           renderer->GetDevice()->GetGraphicsQueue());
    
    m_depthBuffer = std::make_unique<VulkanDepthBuffer>();
    if (!m_depthBuffer->Initialize(device, VkExtent2D{width, height}))
    {
        PrintError("Failed to create offscreen depth buffer");
    }
    
    p_isLoaded = true;
    p_sendToGPU = true;
}

void RenderTargetTexture::Resize(VulkanRenderer* renderer, uint32_t width, uint32_t height, VkFilter filter)
{
    Unload();
    CreateRenderTarget(renderer, width, height, filter);
}
