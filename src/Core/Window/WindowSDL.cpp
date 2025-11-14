#ifdef WINDOW_API_SDL
#include "WindowSDL.h"
#include <iostream>

#ifdef RENDER_API_VULKAN
#include <SDL2/SDL_vulkan.h>
#endif

// #define STB_IMAGE_IMPLEMENTATION
// #include <stb_image.h>

WindowSDL::~WindowSDL()
{
    Terminate();
}

bool WindowSDL::Initialize(RenderAPI renderAPI, const WindowConfig& config)
{
    if (s_windowCount == 0)
    {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
        {
            std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
            return false;
        }
        else
        {
            SDL_version version;
            SDL_GetVersion(&version);
            std::cout << "SDL version: " << version.major << "." << version.minor << "." << version.patch << std::endl;
        }
    }

    Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;

    // Configure SDL based on render API
    switch (renderAPI)
    {
        case RenderAPI::OpenGL:
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            windowFlags |= SDL_WINDOW_OPENGL;
            break;
        case RenderAPI::Vulkan:
            windowFlags |= SDL_WINDOW_VULKAN;
            break;
        case RenderAPI::DirectX:
            // DirectX typically requires platform-specific setup
            break;
        default:
            break;
    }

    p_windowHandle = SDL_CreateWindow(
        config.title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        config.size.x,
        config.size.y,
        windowFlags
    );

    if (!p_windowHandle)
    {
        std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
        if (s_windowCount == 0) SDL_Quit();
        return false;
    }

    s_windowCount++;

    if (renderAPI == RenderAPI::OpenGL)
    {
        m_glContext = SDL_GL_CreateContext(GetHandle());
        if (!m_glContext)
        {
            std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(GetHandle());
            p_windowHandle = nullptr;
            s_windowCount--;
            if (s_windowCount == 0) SDL_Quit();
            return false;
        }
        SDL_GL_MakeCurrent(GetHandle(), m_glContext);
    }

    CreateCursors();
    return true;
}

void WindowSDL::Terminate()
{
    if (p_windowHandle)
    {
        DestroyCursors();
        
        if (m_glContext)
        {
            SDL_GL_DeleteContext(m_glContext);
            m_glContext = nullptr;
        }
        
        SDL_DestroyWindow(GetHandle());
        p_windowHandle = nullptr;
        
        s_windowCount--;
        if (s_windowCount == 0)
        {
            SDL_Quit();
        }
    }
}

void WindowSDL::WaitEvents()
{
    SDL_WaitEvent(nullptr);
}

VkSurfaceKHR WindowSDL::CreateSurface(VkInstance instance)
{
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(GetHandle(), instance, &surface))
    {
        throw std::runtime_error("Failed to create Vulkan surface with SDL2!");
    }
    return surface;
}

void WindowSDL::SetupCallbacks() const
{
    
}

void WindowSDL::CreateCursors()
{
    m_cursors[CursorType::Arrow] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    m_cursors[CursorType::IBeam] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
    m_cursors[CursorType::CrossHair] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
    m_cursors[CursorType::Hand] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
    m_cursors[CursorType::HResize] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
    m_cursors[CursorType::WResize] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
}

void WindowSDL::DestroyCursors()
{
    for (auto& [type, cursor] : m_cursors)
    {
        if (cursor)
        {
            SDL_FreeCursor(cursor);
        }
    }
    m_cursors.clear();
}

void WindowSDL::SetTitle(const std::string& title)
{
    SDL_SetWindowTitle(GetHandle(), title.c_str());
}

void WindowSDL::SetSize(const Vec2i& size)
{
    SDL_SetWindowSize(GetHandle(), size.x, size.y);
}

void WindowSDL::SetPosition(const Vec2i& position)
{
    SDL_SetWindowPosition(GetHandle(), position.x, position.y);
}

void WindowSDL::SetIcon(const std::filesystem::path& icon)
{
    (void)icon;
    //TODO
    // int width, height, channels;
    // unsigned char* data = stbi_load(icon.string().c_str(), &width, &height, &channels, 4);
    //
    // if (data)
    // {
    //     SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
    //         data,
    //         width,
    //         height,
    //         32,
    //         width * 4,
    //         0x000000ff,
    //         0x0000ff00,
    //         0x00ff0000,
    //         0xff000000
    //     );
    //     
    //     if (surface)
    //     {
    //         SDL_SetWindowIcon(GetHandle(), surface);
    //         SDL_FreeSurface(surface);
    //     }
    //     
    //     stbi_image_free(data);
    // }
}

void WindowSDL::SetVSync(bool enabled)
{
    m_vsync = enabled;
    if (m_glContext)
    {
        SDL_GL_SetSwapInterval(enabled ? 1 : 0);
    }
    else
    {
        std::cerr << "VSync is not supported for this render API" << std::endl;
    }
}

void WindowSDL::Close(bool shouldClose)
{
    m_shouldClose = shouldClose;
}

void WindowSDL::SetMouseCursorPositionScreen(const Vec2f& position)
{
    SDL_WarpMouseInWindow(GetHandle(), static_cast<int>(position.x), static_cast<int>(position.y));
}

void WindowSDL::SetMouseCursorMode(CursorMode mode)
{
    m_cursorMode = mode;
    
    switch (mode)
    {
        case CursorMode::Normal:
            SDL_ShowCursor(SDL_ENABLE);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            break;
        case CursorMode::Hidden:
            SDL_ShowCursor(SDL_DISABLE);
            SDL_SetRelativeMouseMode(SDL_FALSE);
            break;
        case CursorMode::Disabled:
            SDL_SetRelativeMouseMode(SDL_TRUE);
            break;
    }
}

void WindowSDL::SetMouseCursorType(CursorType type)
{
    m_cursorType = type;
    auto it = m_cursors.find(type);
    if (it != m_cursors.end())
    {
        SDL_SetCursor(it->second);
    }
}

std::string WindowSDL::GetTitle() const
{
    return SDL_GetWindowTitle(GetHandle());
}

Vec2i WindowSDL::GetSize() const
{
    int width, height;
    SDL_GetWindowSize(GetHandle(), &width, &height);
    return Vec2i(width, height);
}

Vec2i WindowSDL::GetPosition() const
{
    int x, y;
    SDL_GetWindowPosition(GetHandle(), &x, &y);
    return Vec2i(x, y);
}

bool WindowSDL::GetVSync() const
{
    return m_vsync;
}

CursorMode WindowSDL::GetMouseCursorMode() const
{
    return m_cursorMode;
}

CursorType WindowSDL::GetMouseCursorType() const
{
    return m_cursorType;
}

Vec2f WindowSDL::GetMouseCursorPositionScreen() const
{
    int x, y;
    SDL_GetMouseState(&x, &y);
    return Vec2f(static_cast<float>(x), static_cast<float>(y));
}

bool WindowSDL::IsVSyncEnabled() const
{
    return m_vsync;
}

bool WindowSDL::ShouldClose() const
{
    return m_shouldClose;
}

std::vector<const char*> WindowSDL::GetRequiredExtensions() const
{
#ifdef RENDER_API_VULKAN
    unsigned int count = 0;
    SDL_Vulkan_GetInstanceExtensions(GetHandle(), &count, NULL);
    const char** extensions = static_cast<const char**>(malloc(sizeof(char*) * count));
    SDL_Vulkan_GetInstanceExtensions(GetHandle(), &count, extensions);

    std::vector<const char*> requiredExtensions;
    requiredExtensions.reserve(count);
    for (uint32_t i = 0; i < count; ++i)
    {
        requiredExtensions.emplace_back(extensions[i]);
    }
    return requiredExtensions;
#endif
    return {};
}

void* WindowSDL::GetNativeHandle() const
{
    throw std::runtime_error("Not implemented");
}

void WindowSDL::PollEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        ProcessEvent(event);

        if (m_shouldClose)
        {
            break;
        }
    }
}

void WindowSDL::ProcessEvent(const SDL_Event& event)
{
    std::cout << "Event: " << event.type << std::endl;
    switch (event.type)
    {
        case SDL_QUIT:
            m_shouldClose = true;
            break;
            
        case SDL_WINDOWEVENT:
            switch (event.window.event)
            {
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    EResizeEvent.Invoke(Vec2i(event.window.data1, event.window.data2));
                    break;
                    
                case SDL_WINDOWEVENT_MOVED:
                    EMoveEvent.Invoke(Vec2i(event.window.data1, event.window.data2));
                    break;
            }
            break;
            
        case SDL_DROPFILE:
        {
            const char* dropped = event.drop.file;
            EDropCallback.Invoke(1, &dropped);
            SDL_free((void*)dropped);
            break;
        }
            
        case SDL_KEYDOWN:
        {
            KeyEvent keyEvent = event.key.repeat ? KeyEvent::Down : KeyEvent::Pressed;
            EKeyCallback.Invoke(event.key.keysym.scancode, keyEvent);
            break;
        }
            
        case SDL_KEYUP:
            EKeyCallback.Invoke(event.key.keysym.scancode, KeyEvent::Released);
            break;
            
        case SDL_MOUSEBUTTONDOWN:
            EMouseButtonCallback.Invoke(event.button.button, KeyEvent::Pressed);
            break;
            
        case SDL_MOUSEBUTTONUP:
            EMouseButtonCallback.Invoke(event.button.button, KeyEvent::Released);
            break;
            
        case SDL_MOUSEWHEEL:
            EScrollCallback.Invoke(static_cast<double>(event.wheel.x), static_cast<double>(event.wheel.y));
            break;
    }
}
#endif