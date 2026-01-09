#pragma once
#include "IResource.h"
#include "Loader/ImageLoader.h"
#include "Render/Vulkan/VulkanTexture.h"

class Texture : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(Texture)

    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(VulkanRenderer* renderer) override;
    void Unload() override;
    
    VulkanTexture* GetBuffer() const { return m_buffer.get(); }
private:
    ImageLoader::Image m_image = {};
    std::unique_ptr<VulkanTexture> m_buffer;
};
