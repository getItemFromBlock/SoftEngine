#pragma once
#ifdef WINDOW_API_SDL
#include "Core/Window.h"
#include <SDL2/SDL.h>
#include <unordered_map>

class WindowSDL : public Window
{
public:
    WindowSDL() = default;
    ~WindowSDL() override;

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

    SDL_Window* GetHandle() const { return static_cast<SDL_Window*>(p_windowHandle); }
    void* GetNativeHandle() const override;
    SDL_GLContext GetGLContext() const { return m_glContext; }

    void WaitEvents() override;
    
#ifdef RENDER_API_VULKAN
    VkSurfaceKHR CreateSurface(VkInstance instance) override;
#endif

private:
    void SetupCallbacks() const override;
    
    void CreateCursors();
    void DestroyCursors();
    void ProcessEvent(const SDL_Event& event);

private:
    SDL_GLContext m_glContext = nullptr;
    bool m_vsync = false;
    bool m_shouldClose = false;
    CursorMode m_cursorMode = CursorMode::Normal;
    CursorType m_cursorType = CursorType::Arrow;
    std::unordered_map<CursorType, SDL_Cursor*> m_cursors;
    
    static inline int s_windowCount = 0;
};
#endif