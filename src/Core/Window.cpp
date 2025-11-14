#include "Window.h"
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
        std::cout << "Window created successfully" << std::endl;
        return window;
    }
    
    return nullptr;
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