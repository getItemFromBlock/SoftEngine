#pragma once
#include "Core/Window.h"
#include <GLFW/glfw3.h>
#include <unordered_map>

#ifdef RENDER_API_VULKAN
#define GLFW_EXPOSE_NATIVE_VULKAN
#endif
#ifdef RENDER_API_OPENGL
#define GLFW_EXPOSE_NATIVE_OPENGL
#endif
#ifdef RENDER_API_DIRECTX
#define GLFW_EXPOSE_NATIVE_DIRECTX
#endif
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

class WindowGLFW : public Window
{
public:
    WindowGLFW() = default;
    ~WindowGLFW() override;

    bool InitializeAPI() override;
    bool Initialize(RenderAPI renderAPI, const WindowConfig& config) override;
    void PollEvents() override;
    void Terminate() override;

    // === Setters === //
    void SetTitle(const std::string& title) override;
    void SetSize(const Vec2i& size) override;
    void SetPosition(const Vec2i& position) override;
    void SetIcon(const std::filesystem::path& icon) override;
    void SetVSync(bool enabled) override;
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
    bool ShouldClose() const override;

    std::vector<const char*> GetRequiredExtensions() const override;

    GLFWwindow* GetHandle() const { return static_cast<GLFWwindow*>(p_windowHandle); }
    void* GetNativeHandle() const override;

    void WaitEvents() override;
    
#ifdef RENDER_API_VULKAN
    VkSurfaceKHR CreateSurface(VkInstance instance) override;
#endif

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