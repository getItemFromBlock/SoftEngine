#pragma once
#include <memory>

class Window;

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
    
};
