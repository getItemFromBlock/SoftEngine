#pragma once

#include <galaxymath/Maths.h>
#include <filesystem>

#include "Utils/Event.h"
#include "Core/Input.h"
#include "Render/Vulkan/VulkanRenderer.h"

#include <vulkan/vulkan.hpp>

enum class ENGINE_API WindowAPI
{
    GLFW,
};

enum class CoordinateSpace
{
    Window,
    Screen
};

enum ENGINE_API WindowAttributes : uint32_t
{
    None = 0,
    VSync = 1 << 1,
    ClickThrough = 1 << 2,
    NoDecoration = 1 << 3,
    Transparent = 1 << 4
};

struct ENGINE_API WindowConfig
{
    std::string title;
    Vec2i size;
    WindowAttributes attributes = WindowAttributes::None;
};

class ENGINE_API Window
{
public:
    Window() = default;
    Window& operator=(const Window& other) = default;
    Window(const Window&) = default;
    Window(Window&&) noexcept = default;
    virtual ~Window() = default;

    static std::unique_ptr<Window> Create(WindowAPI windowAPI, const WindowConfig& config);
    static std::unique_ptr<Window> Create(const WindowConfig& config);
    
    virtual bool InitializeAPI() = 0;
    
    void InitializeInputs();
    
    virtual bool Initialize(const WindowConfig& config) = 0;
    virtual void PollEvents() = 0;
    virtual void Terminate() = 0;

    WindowAPI GetWindowAPI() const { return p_windowAPI; }

    // === Setters === //
    virtual void SetTitle(const std::string& title) = 0;
    virtual void SetSize(const Vec2i& size) = 0;
    virtual void SetPosition(const Vec2i& position) = 0;
    virtual void SetIcon(const std::filesystem::path& icon) = 0;
    
    virtual void SetVSync(bool enabled) = 0;
    virtual void SetClickThrough(bool enabled) = 0;
    virtual void SetDecorated(bool enabled) = 0;
    virtual void SetTransparent(bool enabled) = 0;
    
    virtual void Close(bool shouldClose) = 0;

    virtual void WaitEvents() = 0;
    void SetMouseCursorPosition(const Vec2i& position, CoordinateSpace space = CoordinateSpace::Window);
    
    virtual void SetMouseCursorPositionScreen(const Vec2f& position) = 0;
    virtual void SetMouseCursorMode(CursorMode mode) = 0;
    virtual void SetMouseCursorType(CursorType type) = 0;

    // === Getters === //
    virtual std::string GetTitle() const = 0;
    float GetAspectRatio() const;
    virtual Vec2i GetSize() const = 0;
    virtual Vec2i GetPosition() const = 0;
    virtual bool GetVSync() const = 0;
    virtual CursorMode GetMouseCursorMode() const = 0;
    virtual CursorType GetMouseCursorType() const = 0;
    virtual Vec2f GetMouseCursorPositionScreen() const = 0;
    
    Vec2i GetMouseCursorPosition(CoordinateSpace space = CoordinateSpace::Window) const;

    virtual std::vector<const char*> GetRequiredExtensions() const = 0;

    virtual bool IsVSyncEnabled() const = 0;
    virtual bool IsClickThroughEnabled() const = 0;
    virtual bool IsDecoratedEnabled() const = 0;
    virtual bool IsTransparentEnabled() const = 0;
    
    virtual bool ShouldClose() const = 0;
    
    Vec2i ToWindowSpace(const Vec2i& pos) const;
    Vec2i ToScreenSpace(const Vec2i& pos) const;

    virtual void* GetNativeHandle() const = 0;

    Input& GetInput() { return p_input; }
    
    virtual VkSurfaceKHR CreateSurface(VkInstance instance) = 0;
public:
    // === Events === //
    Event<Vec2i> EResizeEvent;
    Event<Vec2i> EMoveEvent;
    Event<int, const char**> EDropCallback;
    Event<Key, KeyEvent> EKeyCallback;
    Event<MouseButton, KeyEvent> EMouseButtonCallback;
    Event<double, double> EScrollCallback;
    Event<double, double> EScaleCallback;
    Event<const char*> EErrorCallback;

protected:
    virtual void SetupCallbacks() const = 0;
    
protected:
    void* p_windowHandle = nullptr;
    WindowAPI p_windowAPI;
    Input p_input;
};
