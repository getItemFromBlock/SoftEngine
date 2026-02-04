#pragma once
#include "ComputeShader.h"
#include "IResource.h"
#include "Loader/ImageLoader.h"
#include "Render/Vulkan/VulkanTexture.h"

class CubeMap : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(CubeMap)
    
    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(VulkanRenderer* renderer) override;
    VulkanTexture* GetPrefilteredCubemap() const;
    bool IsPrefilteredReady() const;
    void Unload() override;

    VulkanTexture* GetBuffer() const { return m_buffer.get(); }
private:
    ImageLoader::HDRImage m_image;
    std::unique_ptr<VulkanTexture> m_buffer;
    
    std::unique_ptr<VulkanTexture> m_prefilteredCubemap = VK_NULL_HANDLE;
    std::unique_ptr<ComputeDispatch> m_compute;
    bool m_prefilteredReady = false;
};
