#pragma once
#include "EngineAPI.h"
#include <memory>

#include "Core/Window.h"
#include "Resource/ResourceManager.h"
#include "Render/RHI/RHIRenderer.h"
#include "Scene/ComponentHandler.h"
#include "Scene/SceneHolder.h"

struct ENGINE_API EngineDesc
{
    Window* window;
};

class ENGINE_API Engine
{
public:
    static Engine* Create();
    
    bool Initialize(EngineDesc desc);
    bool BeginFrame();
    void Update();
    void Render();
    void EndFrame();
    void WaitBeforeClean();
    void Cleanup();
    
    static Engine* Get();

    Window* GetWindow() const { return m_window; }
    RHIRenderer* GetRenderer() const { return m_renderer.get(); }
    SceneHolder* GetSceneHolder() const { return m_sceneHolder.get(); }
private:
    Window* m_window;
    std::unique_ptr<RHIRenderer> m_renderer;
    std::unique_ptr<ResourceManager> m_resourceManager;
    std::unique_ptr<ComponentRegister> m_componentRegister;
    std::unique_ptr<SceneHolder> m_sceneHolder;
    
    inline static std::unique_ptr<Engine> s_instance = nullptr;
    
    float m_deltaTime = 0.0f;
};
