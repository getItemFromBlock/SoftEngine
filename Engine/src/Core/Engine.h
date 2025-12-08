#pragma once
#include <memory>

#include "Resource/ResourceManager.h"
#include "Core/Window.h"
#include "Render/RHI/RHIRenderer.h"
#include "Scene/ComponentHandler.h"

class Engine
{
public:
    static void Create();

    bool Initialize();
    void Run();
    void Cleanup() const;
    
    static Engine* Get();

    Window* GetWindow() const { return m_window.get(); }
    RHIRenderer* GetRenderer() const { return m_renderer.get(); }
    
private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<RHIRenderer> m_renderer;
    std::unique_ptr<ResourceManager> m_resourceManager;
    std::unique_ptr<ComponentRegister> m_componentRegister;
    
    inline static std::unique_ptr<Engine> s_instance = nullptr;
};