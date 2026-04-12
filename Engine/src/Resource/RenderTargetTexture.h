#pragma once
#include "Texture.h"
#include "Render/Vulkan/VulkanDepthBuffer.h"

class RenderTargetTexture : public Texture
{
public:
    DECLARE_RESOURCE_TYPE_PARENT(RenderTargetTexture, Texture)
    
    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(VulkanRenderer* renderer) override;
    void Unload() override;
    
    void CreateRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height, VkFilter filter = VK_FILTER_LINEAR, bool createDepthBuffer = true);
    void Resize(VulkanRenderer* renderer, uint32_t width, uint32_t height, VkFilter filter = VK_FILTER_LINEAR);
    
    VulkanDepthBuffer* GetDepthBuffer() const { return m_depthBuffer.get(); }
protected:
    std::unique_ptr<VulkanDepthBuffer> m_depthBuffer;
};
