#pragma once

#include <galaxymath/Maths.h>

#include "Utils/Event.h"
#include "Core/Input.h"
#include "Render/RHI/RHIRenderer.h"

#ifdef RENDER_API_VULKAN
#include <vulkan/vulkan.hpp>
#endif

enum class WindowAPI
{
    GLFW,
    SDL,
};

enum class CoordinateSpace
{
    Window,
    Screen
};

struct WindowConfig
{
    std::string title;
    Vec2i size;
};

class Window
{
public:
    Window() = default;
    Window& operator=(const Window& other) = default;
    Window(const Window&) = default;
    Window(Window&&) noexcept = default;
    virtual ~Window() = default;

    static std::unique_ptr<Window> Create(WindowAPI windowAPI, RenderAPI renderAPI, const WindowConfig& config);
    static std::unique_ptr<Window> Create(const WindowConfig& config);
    
    virtual bool InitializeAPI() = 0;
    
    virtual bool Initialize(RenderAPI renderAPI, const WindowConfig& config) = 0;
    virtual void PollEvents() = 0;
    virtual void Terminate() = 0;

    RenderAPI GetRenderAPI() const { return p_renderAPI; }
    WindowAPI GetWindowAPI() const { return p_windowAPI; }

    // === Setters === //
    virtual void SetTitle(const std::string& title) = 0;
    virtual void SetSize(const Vec2i& size) = 0;
    virtual void SetPosition(const Vec2i& position) = 0;
    virtual void SetIcon(const std::filesystem::path& icon) = 0;
    
    virtual void SetVSync(bool enabled) = 0;
    virtual void Close(bool shouldClose) = 0;

    virtual void WaitEvents() = 0;
    void SetMouseCursorPosition(const Vec2i& position, CoordinateSpace space = CoordinateSpace::Window);
    
    virtual void SetMouseCursorPositionScreen(const Vec2f& position) = 0;
    virtual void SetMouseCursorMode(CursorMode mode) = 0;
    virtual void SetMouseCursorType(CursorType type) = 0;

    // === Getters === //
    virtual std::string GetTitle() const = 0;
    virtual Vec2i GetSize() const = 0;
    virtual Vec2i GetPosition() const = 0;
    virtual bool GetVSync() const = 0;
    virtual CursorMode GetMouseCursorMode() const = 0;
    virtual CursorType GetMouseCursorType() const = 0;
    virtual Vec2f GetMouseCursorPositionScreen() const = 0;
    Vec2i GetMouseCursorPosition(CoordinateSpace space = CoordinateSpace::Window) const;

    virtual std::vector<const char*> GetRequiredExtensions() const = 0;

    virtual bool IsVSyncEnabled() const = 0;
    virtual bool ShouldClose() const = 0;
    
    Vec2i ToWindowSpace(const Vec2i& pos) const;
    Vec2i ToScreenSpace(const Vec2i& pos) const;

    virtual void* GetNativeHandle() const = 0;

#ifdef RENDER_API_VULKAN
    virtual VkSurfaceKHR CreateSurface(VkInstance instance) = 0;
#endif
public:
    // === Events === //
    Event<Vec2i> EResizeEvent;
    Event<Vec2i> EMoveEvent;
    Event<int, const char**> EDropCallback;
    Event<int, KeyEvent> EKeyCallback;
    Event<int, KeyEvent> EMouseButtonCallback;
    Event<double, double> EScrollCallback;
    Event<double, double> EScaleCallback;
    Event<const char*> EErrorCallback;

protected:
    virtual void SetupCallbacks() const = 0;
    
protected:
    void* p_windowHandle = nullptr;
    WindowAPI p_windowAPI;
    RenderAPI p_renderAPI;
};
