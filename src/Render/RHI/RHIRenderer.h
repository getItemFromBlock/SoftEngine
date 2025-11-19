#pragma once
#include <memory>

#include "RHIIndexBuffer.h"
#include "RHITexture.h"
#include "RHIVertexBuffer.h"

class Window;

namespace ImageLoader
{
    struct Image;
}

enum class RenderAPI
{
    None,
    Vulkan,
    DirectX,
    OpenGL,
    Metal,
};

class RHIRenderer
{
public:
    RHIRenderer() = default;
    virtual ~RHIRenderer();

    static std::unique_ptr<RHIRenderer> Create(RenderAPI api, Window* window);

    virtual bool Initialize(Window* window) = 0;
    virtual void Cleanup() = 0;
    
    virtual void DrawFrame() = 0;
    virtual bool MultiThreadSendToGPU() = 0;
    
    virtual std::unique_ptr<RHITexture> CreateTexture(const ImageLoader::Image& image) = 0;
    virtual std::unique_ptr<RHIVertexBuffer> CreateVertexBuffer(const float* data, uint32_t size, uint32_t floatPerVertex) = 0;
    virtual std::unique_ptr<RHIIndexBuffer> CreateIndexBuffer(const uint32_t* data, uint32_t size) = 0;
    
private:
    RenderAPI m_renderAPI;
};
