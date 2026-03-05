#pragma once
#include "IResource.h"
#include "Loader/ImageLoader.h"
#include "Render/Vulkan/VulkanTexture.h"

enum class TextureFormat { SRGB, UNORM, R8_UNORM };

enum class TextureFilter { LINEAR, NEAREST };

struct TextureParam
{
    TextureFormat format = TextureFormat::SRGB;
    TextureFilter filter = TextureFilter::LINEAR;
    
    bool operator==(const TextureParam& o) const
    {
        return format == o.format && filter == o.filter;
    }
};

class Texture : public IResource
{
public:
    DECLARE_RESOURCE_TYPE(Texture)

    virtual bool Load(ResourceManager* resourceManager) override;
    virtual bool SendToGPU(VulkanRenderer* renderer) override;
    virtual void Unload() override;

    void Describe(ClassDescriptor& descriptor) override;

    void CreateFromBuffer(const GBufferAttachment& attachment, VkSampler sampler, uint32_t width, uint32_t height);
    void CreateFromData(const ImageLoader::Image& image);

    VulkanTexture* GetBuffer() const { return m_buffer.get(); }
    
    void SetTextureParameters(TextureParam param);

protected:
    ImageLoader::Image m_image = {};
    std::unique_ptr<VulkanTexture> m_buffer;
    TextureParam m_parameters;
};
