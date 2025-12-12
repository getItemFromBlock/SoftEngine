#pragma once
#include "EngineAPI.h"
#include <memory>

#include "Resource/ResourceManager.h"
#include "Core/Window.h"
#include "Render/RHI/RHIRenderer.h"
#include "Scene/ComponentHandler.h"
#include "Scene/Scene.h"

#include "Utils/Event.h"

class ENGINE_API Engine
{
public:
    static void Create();

    bool Initialize();
    void Run();
    void Cleanup();
    
    static Engine* Get();

    Window* GetWindow() const { return m_window.get(); }
    RHIRenderer* GetRenderer() const { return m_renderer.get(); }

    Event<> OnRender;
    Event<> OnCleanup;
    Event<> OnEndFrame;
private:
    bool m_ready = false;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<RHIRenderer> m_renderer;
    std::unique_ptr<ResourceManager> m_resourceManager;
    std::unique_ptr<ComponentRegister> m_componentRegister;
    std::unique_ptr<Scene> m_scene;
    
    inline static std::unique_ptr<Engine> s_instance = nullptr;
};
