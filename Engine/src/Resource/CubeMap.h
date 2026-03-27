#pragma once
#include "ComputeShader.h"
#include "IResource.h"
#include "Loader/ImageLoader.h"
#include "Render/Vulkan/VulkanTexture.h"


class CubeMap : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(CubeMap)
    
    enum class SampleMode { Environment, Irradiance, Prefilter };
    
    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(VulkanRenderer* renderer) override;
    
    bool GenerateIrradiance(VulkanRenderer* renderer, uint32_t resolution = 32);
    bool GeneratePrefiltered(VulkanRenderer* renderer, uint32_t resolution = 512, uint32_t mipLevels = 5);
    bool GenerateBRDFLut(VulkanRenderer* renderer, uint32_t resolution = 256);

    VulkanTexture* GetPrefilteredCubemap() const;
    bool IsPrefilteredReady() const;
    void Unload() override;
    
    void SetSampleMode(SampleMode mode) { m_sampleMode = mode; }
    SampleMode GetSampleMode() const { return m_sampleMode; }

    VulkanTexture* GetBuffer() const;
    
    SafePtr<Texture> GetBRDFLutTexture() const { return m_brdfLutTexture; }

private:
    ImageLoader::HDRImage m_image;
    std::unique_ptr<VulkanTexture> m_buffer;
    SampleMode m_sampleMode = SampleMode::Environment;
    
    std::unique_ptr<VulkanTexture> m_prefilteredCubemap = VK_NULL_HANDLE;
    std::unique_ptr<VulkanTexture> m_irradianceMap = VK_NULL_HANDLE;
    std::unique_ptr<VulkanTexture> m_brdfLut = VK_NULL_HANDLE;
    
    std::unique_ptr<ComputeDispatch> m_prefilteredCompute;
    std::unique_ptr<ComputeDispatch> m_irradianceCompute;
    std::unique_ptr<ComputeDispatch> m_brdfLutCompute;
    
    std::shared_ptr<Texture> m_brdfLutTexture;
    
    bool m_prefilteredReady = false;
    bool m_irradianceReady = false;
    bool m_brdfLutReady = false;
};
