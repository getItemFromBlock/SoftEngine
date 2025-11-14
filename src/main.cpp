#include <iostream>

#include "Core/Window.h"
#include "Render/RHI/RHIRenderer.h"

int main() {
    
    WindowConfig config;
    config.title = "Window Test";
    config.size = Vec2i(1280, 720);
    std::unique_ptr<Window> window = Window::Create(WindowAPI::SDL, RenderAPI::Vulkan, config);

    if (!window)
    {
        std::cerr << "Failed to create window" << std::endl;
        return -1;
    }
    
    std::unique_ptr<RHIRenderer> renderer = RHIRenderer::Create(RenderAPI::Vulkan, window.get());
    if (!renderer)
    {
        std::cerr << "Failed to create renderer" << std::endl;
        return -1;
    }
    
    window->SetVSync(true);
    window->SetMouseCursorType(CursorType::Hand);

    while (!window->ShouldClose())
    {
        window->PollEvents();

        renderer->DrawFrame();
    }

    window->Terminate();
    return 0;
}