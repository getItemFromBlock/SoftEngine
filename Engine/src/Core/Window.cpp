#include "Window.h"

#include "Debug/Log.h"
#include "Window/WindowGLFW.h"
#include "Window/WindowSDL.h"

std::unique_ptr<Window> Window::Create(WindowAPI windowAPI, RenderAPI renderAPI, const WindowConfig& config)
{
    std::unique_ptr<Window> window;
    
    switch (windowAPI)
    {
    case WindowAPI::GLFW:
        window = std::make_unique<WindowGLFW>();
        break;
    case WindowAPI::SDL:
#ifdef WINDOW_API_SDL
        window = std::make_unique<WindowSDL>();
#else
        return nullptr; // SDL is not enabled
#endif
        break;
    default:
        return nullptr;
    }

    window->p_windowAPI = windowAPI;
    window->p_renderAPI = renderAPI;
    
    if (window && window->Initialize(renderAPI, config))
    {
        window->InitializeInputs();
        
        PrintLog("Window created successfully");
        return window;
    }
    
    return nullptr;
}

std::unique_ptr<Window> Window::Create(const WindowConfig& config)
{
#ifndef RENDER_API_VULKAN
    RenderAPI renderAPI = RenderAPI::OpenGL;
#else
    RenderAPI renderAPI = RenderAPI::Vulkan;
#endif
    
#ifdef WINDOW_API_GLFW
    WindowAPI windowAPI = WindowAPI::GLFW;
#else
    WindowAPI windowAPI = WindowAPI::SDL;
#endif
    
    return Create(windowAPI, renderAPI, config);
}

void Window::InitializeInputs()
{
    EKeyCallback.Bind([this](Key key, KeyEvent state)
    {
        p_input.OnKeyCallback(key, state);
    });
    
    EMouseButtonCallback.Bind([this](MouseButton button, KeyEvent state)
    {
        p_input.OnMouseButtonCallback(button, state);
    });
}

void Window::SetMouseCursorPosition(const Vec2i& position, CoordinateSpace space)
{
    if (space == CoordinateSpace::Window)
    {
        SetMouseCursorPositionScreen(Vec2f(ToScreenSpace(position)));
    }
    else
    {
        SetMouseCursorPositionScreen(Vec2f(position));
    }
}

float Window::GetAspectRatio() const
{
    Vec2i size = GetSize();
    return static_cast<float>(size.x) / static_cast<float>(size.y);
}

Vec2i Window::GetMouseCursorPosition(CoordinateSpace space) const
{
    Vec2f screenPos = GetMouseCursorPositionScreen();
    Vec2i pos(static_cast<int>(screenPos.x), static_cast<int>(screenPos.y));
    
    if (space == CoordinateSpace::Window)
    {
        return ToWindowSpace(pos);
    }
    return pos;
}

Vec2i Window::ToWindowSpace(const Vec2i& pos) const
{
    Vec2i windowPos = GetPosition();
    return Vec2i(pos.x - windowPos.x, pos.y - windowPos.y);
}

Vec2i Window::ToScreenSpace(const Vec2i& pos) const
{
    Vec2i windowPos = GetPosition();
    return Vec2i(pos.x + windowPos.x, pos.y + windowPos.y);
}