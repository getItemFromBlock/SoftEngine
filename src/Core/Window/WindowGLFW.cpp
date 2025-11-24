#include "WindowGLFW.h"

#include <filesystem>
#include <iostream>
#include <GLFW/glfw3native.h>

#include "Debug/Log.h"
#include "Resource/Loader/ImageLoader.h"

// #define STB_IMAGE_IMPLEMENTATION
// #include <stb_image.h>

WindowGLFW::~WindowGLFW()
{
    Terminate();
}

bool WindowGLFW::InitializeAPI()
{
    if (!glfwInit())
    {
        PrintError("Failed to initialize GLFW");
        return false;
    }
    else
    {
        int major, minor, rev;
        glfwGetVersion(&major, &minor, &rev);
        PrintLog("GLFW version: %d.%d.%d", major, minor, rev);
    }
    glfwSetErrorCallback(ErrorCallback);
    return true;
}

bool WindowGLFW::Initialize(RenderAPI renderAPI, const WindowConfig& config)
{
    if (s_windowCount == 0)
    {
        InitializeAPI();
    }

    // Configure GLFW based on render API
    switch (renderAPI)
    {
        case RenderAPI::OpenGL:
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            #ifdef __APPLE__
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
            #endif
            break;
        case RenderAPI::Vulkan:
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            break;
        case RenderAPI::DirectX:
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            break;
        default:
            break;
    }

    p_windowHandle = glfwCreateWindow(config.size.x, config.size.y, config.title.c_str(), nullptr, nullptr);
    if (!p_windowHandle)
    {
        PrintError("Failed to create GLFW window");
        if (s_windowCount == 0) 
            glfwTerminate();
        return false;
    }

    s_windowCount++;
    
    if (renderAPI == RenderAPI::OpenGL)
    {
        glfwMakeContextCurrent(GetHandle());
    }

    glfwSetWindowUserPointer(GetHandle(), this);
    SetupCallbacks();
    CreateCursors();

    return true;
}

void WindowGLFW::PollEvents()
{
    glfwPollEvents();
}

void WindowGLFW::Terminate()
{
    if (p_windowHandle)
    {
        DestroyCursors();
        glfwDestroyWindow(GetHandle());
        p_windowHandle = nullptr;
        
        s_windowCount--;
        if (s_windowCount == 0)
        {
            glfwTerminate();
        }
    }
}

void* WindowGLFW::GetNativeHandle() const
{
#ifdef _WIN32
    return glfwGetWin32Window(GetHandle());
#else
    return glfwGetX11Window(GetHandle());
#endif
}

void WindowGLFW::WaitEvents()
{
    glfwWaitEvents();
}

VkSurfaceKHR WindowGLFW::CreateSurface(VkInstance instance)
{
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, GetHandle(), nullptr, &surface) != VK_SUCCESS)
    {
        PrintError("Failed to create window surface");
    }
    return surface;
}

void WindowGLFW::SetupCallbacks() const
{
    glfwSetFramebufferSizeCallback(GetHandle(), FramebufferSizeCallback);
    glfwSetWindowPosCallback(GetHandle(), WindowPosCallback);
    glfwSetDropCallback(GetHandle(), DropCallback);
    glfwSetKeyCallback(GetHandle(), KeyCallback);
    glfwSetMouseButtonCallback(GetHandle(), MouseButtonCallback);
    glfwSetScrollCallback(GetHandle(), ScrollCallback);
    glfwSetWindowContentScaleCallback(GetHandle(), WindowContentScaleCallback);
}

void WindowGLFW::CreateCursors()
{
    m_cursors[CursorType::Arrow] = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    m_cursors[CursorType::IBeam] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    m_cursors[CursorType::CrossHair] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    m_cursors[CursorType::Hand] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    m_cursors[CursorType::HResize] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    m_cursors[CursorType::WResize] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
}

void WindowGLFW::DestroyCursors()
{
    for (auto& [type, cursor] : m_cursors)
    {
        if (cursor)
        {
            glfwDestroyCursor(cursor);
        }
    }
    m_cursors.clear();
}

void WindowGLFW::SetTitle(const std::string& title)
{
    glfwSetWindowTitle(GetHandle(), title.c_str());
}

void WindowGLFW::SetSize(const Vec2i& size)
{
    glfwSetWindowSize(GetHandle(), size.x, size.y);
}

void WindowGLFW::SetPosition(const Vec2i& position)
{
    glfwSetWindowPos(GetHandle(), position.x, position.y);
}

void WindowGLFW::SetIcon(const std::filesystem::path& icon)
{
    ImageLoader::Image image;
    if (!ImageLoader::Load(icon.generic_string(), image))
    {
        PrintError("Failed to load icon %s", icon.generic_string().c_str());
        return;
    }

    GLFWimage glfwImage;
    glfwImage.width = image.size.x;
    glfwImage.height = image.size.y;
    glfwImage.pixels = image.data;

    glfwSetWindowIcon(GetHandle(), 1, &glfwImage);
    ImageLoader::ImageFree(image.data);
}

void WindowGLFW::SetVSync(bool enabled)
{
    if (p_renderAPI == RenderAPI::Vulkan)
    {
        PrintWarning("VSync is not supported for Vulkan");
        return;
    }
    m_vsync = enabled;
    glfwSwapInterval(enabled ? 1 : 0);
}

void WindowGLFW::Close(bool shouldClose)
{
    glfwSetWindowShouldClose(GetHandle(), shouldClose ? GLFW_TRUE : GLFW_FALSE);
}

void WindowGLFW::SetMouseCursorPositionScreen(const Vec2f& position)
{
    glfwSetCursorPos(GetHandle(), position.x, position.y);
}

void WindowGLFW::SetMouseCursorMode(CursorMode mode)
{
    m_cursorMode = mode;
    int glfwMode = GLFW_CURSOR_NORMAL;
    
    switch (mode)
    {
        case CursorMode::Normal:
            glfwMode = GLFW_CURSOR_NORMAL;
            break;
        case CursorMode::Hidden:
            glfwMode = GLFW_CURSOR_HIDDEN;
            break;
        case CursorMode::Disabled:
            glfwMode = GLFW_CURSOR_DISABLED;
            break;
    }
    
    glfwSetInputMode(GetHandle(), GLFW_CURSOR, glfwMode);
}

void WindowGLFW::SetMouseCursorType(CursorType type)
{
    m_cursorType = type;
    auto it = m_cursors.find(type);
    if (it != m_cursors.end())
    {
        glfwSetCursor(GetHandle(), it->second);
    }
}

std::string WindowGLFW::GetTitle() const
{
    return glfwGetWindowTitle(GetHandle());
}

Vec2i WindowGLFW::GetSize() const
{
    int width, height;
    glfwGetWindowSize(GetHandle(), &width, &height);
    return Vec2i(width, height);
}

Vec2i WindowGLFW::GetPosition() const
{
    int x, y;
    glfwGetWindowPos(GetHandle(), &x, &y);
    return Vec2i(x, y);
}

bool WindowGLFW::GetVSync() const
{
    return m_vsync;
}

CursorMode WindowGLFW::GetMouseCursorMode() const
{
    return m_cursorMode;
}

CursorType WindowGLFW::GetMouseCursorType() const
{
    return m_cursorType;
}

Vec2f WindowGLFW::GetMouseCursorPositionScreen() const
{
    double x, y;
    glfwGetCursorPos(GetHandle(), &x, &y);
    return Vec2f(static_cast<float>(x), static_cast<float>(y));
}

bool WindowGLFW::IsVSyncEnabled() const
{
    return m_vsync;
}

bool WindowGLFW::ShouldClose() const
{
    return glfwWindowShouldClose(GetHandle());
}

std::vector<const char*> WindowGLFW::GetRequiredExtensions() const
{
    uint32_t extensionCount = 0;
    const char** extensionsCSTR = glfwGetRequiredInstanceExtensions(&extensionCount);

    std::vector<const char*> extensions(extensionCount);
    for (size_t i = 0; i < extensionCount; i++)
    {
        extensions[i] = extensionsCSTR[i];
    }
    return extensions;
}

// === Callbacks === //

void WindowGLFW::ErrorCallback(int error, const char* description)
{
    PrintError("GLFW Error (%d): %s", error, description);
}

void WindowGLFW::FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    WindowGLFW* win = static_cast<WindowGLFW*>(glfwGetWindowUserPointer(window));
    if (win)
    {
        win->EResizeEvent.Invoke(Vec2i(width, height));
    }
}

void WindowGLFW::WindowPosCallback(GLFWwindow* window, int xpos, int ypos)
{
    WindowGLFW* win = static_cast<WindowGLFW*>(glfwGetWindowUserPointer(window));
    if (win)
    {
        win->EMoveEvent.Invoke(Vec2i(xpos, ypos));
    }
}

void WindowGLFW::DropCallback(GLFWwindow* window, int count, const char** paths)
{
    WindowGLFW* win = static_cast<WindowGLFW*>(glfwGetWindowUserPointer(window));
    if (win)
    {
        win->EDropCallback.Invoke(count, paths);
    }
}

void WindowGLFW::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode;
    (void)mods;
    
    WindowGLFW* win = static_cast<WindowGLFW*>(glfwGetWindowUserPointer(window));
    if (win)
    {
        KeyEvent event = KeyEvent::None;
        switch (action)
        {
            case GLFW_PRESS:
                event = KeyEvent::Pressed;
                break;
            case GLFW_REPEAT:
                event = KeyEvent::Down;
                break;
            case GLFW_RELEASE:
                event = KeyEvent::Released;
                break;
        }
        win->EKeyCallback.Invoke(key, event);
    }
}

void WindowGLFW::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    (void)mods;
    WindowGLFW* win = static_cast<WindowGLFW*>(glfwGetWindowUserPointer(window));
    if (win)
    {
        KeyEvent event = (action == GLFW_PRESS) ? KeyEvent::Pressed : KeyEvent::Released;
        win->EMouseButtonCallback.Invoke(button, event);
    }
}

void WindowGLFW::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    WindowGLFW* win = static_cast<WindowGLFW*>(glfwGetWindowUserPointer(window));
    if (win)
    {
        win->EScrollCallback.Invoke(xoffset, yoffset);
    }
}

void WindowGLFW::WindowContentScaleCallback(GLFWwindow* window, float xscale, float yscale)
{
    WindowGLFW* win = static_cast<WindowGLFW*>(glfwGetWindowUserPointer(window));
    if (win)
    {
        win->EScaleCallback.Invoke(static_cast<double>(xscale), static_cast<double>(yscale));
    }
}