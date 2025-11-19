#pragma once
#include "IResource.h"
#include "Loader/ImageLoader.h"
#include "Render/RHI/RHITexture.h"

class Texture : public IResource
{
public:
    explicit Texture(std::filesystem::path path) : IResource(std::move(path)) {}
    Texture(const Texture&) = delete;
    Texture(Texture&&) = delete;
    Texture& operator=(const Texture&) = delete;
    ~Texture() override = default;

    bool Load(ResourceManager* resourceManager) override;
    bool SendToGPU(RHIRenderer* renderer) override;
    void Unload() override;
    
    RHITexture* GetBuffer() const { return m_buffer.get(); }
private:
    ImageLoader::Image m_image = {};
    std::unique_ptr<RHITexture> m_buffer;
};
