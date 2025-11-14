#include "RHIRenderer.h"

#ifdef RENDER_API_VULKAN
#include "Render/Vulkan/VulkanRenderer.h"
#endif

RHIRenderer::~RHIRenderer() = default;

std::unique_ptr<RHIRenderer> RHIRenderer::Create(RenderAPI api, Window* window)
{
    std::unique_ptr<RHIRenderer> renderer;
    switch (api)
    {
    case RenderAPI::Vulkan:
#ifdef RENDER_API_VULKAN
        renderer = std::make_unique<VulkanRenderer>();
#endif
        break;
    default:
        break;
    }

    if (renderer)
        renderer->Initialize(window);
    
    return renderer;
}
