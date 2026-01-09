#include "Window.h"

#include "Debug/Log.h"
#include "Window/WindowGLFW.h"

std::unique_ptr<Window> Window::Create(WindowAPI windowAPI, const WindowConfig& config)
{
    std::unique_ptr<Window> window;
    
    switch (windowAPI)
    {
    case WindowAPI::GLFW:
        window = std::make_unique<WindowGLFW>();
        break;
    default:
        return nullptr;
    }

    window->p_windowAPI = windowAPI;
    
    if (window && window->Initialize(config))
    {
        window->InitializeInputs();
        
        PrintLog("Window created successfully");
        return window;
    }
    
    return nullptr;
}

std::unique_ptr<Window> Window::Create(const WindowConfig& config)
{
    WindowAPI windowAPI = WindowAPI::GLFW;
    
    return Create(windowAPI, config);
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