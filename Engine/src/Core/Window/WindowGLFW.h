#pragma once
#include "Core/Window.h"
#include <unordered_map>

#define GLFW_EXPOSE_NATIVE_VULKAN
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
// #define GLFW_EXPOSE_NATIVE_COCOA
// #define GLFW_EXPOSE_NATIVE_NSGL
// #define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

class ENGINE_API WindowGLFW : public Window
{
public:
    WindowGLFW() = default;
    ~WindowGLFW() override;

    bool InitializeAPI() override;
    bool Initialize(const WindowConfig& config) override;
    void PollEvents() override;
    void Terminate() override;

    // === Setters === //
    void SetTitle(const std::string& title) override;
    void SetSize(const Vec2i& size) override;
    void SetPosition(const Vec2i& position) override;
    void SetIcon(const std::filesystem::path& icon) override;
    
    void SetVSync(bool enabled) override;
    void SetClickThrough(bool enabled) override;
    void SetDecorated(bool enabled) override;
    void SetTransparent(bool enabled) override;
    
    void Close(bool shouldClose) override;
    void SetMouseCursorPositionScreen(const Vec2f& position) override;
    void SetMouseCursorMode(CursorMode mode) override;
    void SetMouseCursorType(CursorType type) override;

    // === Getters === //
    std::string GetTitle() const override;
    Vec2i GetSize() const override;
    Vec2i GetPosition() const override;
    bool GetVSync() const override;
    CursorMode GetMouseCursorMode() const override;
    CursorType GetMouseCursorType() const override;
    Vec2f GetMouseCursorPositionScreen() const override;
    
    bool IsVSyncEnabled() const override;
    bool IsClickThroughEnabled() const override;
    bool IsDecoratedEnabled() const override;
    bool IsTransparentEnabled() const override;
    
    bool ShouldClose() const override;

    std::vector<const char*> GetRequiredExtensions() const override;

    GLFWwindow* GetHandle() const { return static_cast<GLFWwindow*>(p_windowHandle); }
    void* GetNativeHandle() const override;

    void WaitEvents() override;
    
    // KeyEvent GetKeyState(Key key) const override;
    
    VkSurfaceKHR CreateSurface(VkInstance instance) override;

private:
    void SetupCallbacks() const override;
    void CreateCursors();
    void DestroyCursors();
    
    static void ErrorCallback(int error, const char* description);
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void WindowPosCallback(GLFWwindow* window, int xpos, int ypos);
    static void DropCallback(GLFWwindow* window, int count, const char** paths);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void WindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale);

    bool m_vsync = false;
    CursorMode m_cursorMode = CursorMode::Normal;
    CursorType m_cursorType = CursorType::Arrow;
    std::unordered_map<CursorType, GLFWcursor*> m_cursors;
    
    static inline int s_windowCount = 0;
};