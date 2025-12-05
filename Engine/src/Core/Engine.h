#pragma once
#include <memory>

#include "Resource/ResourceManager.h"
#include "Core/Window.h"
#include "Render/RHI/RHIRenderer.h"
#include "Scene/ComponentHandler.h"

class Engine
{
public:
    bool Initialize();
    void Run();
    void Cleanup() const;
    
private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<RHIRenderer> m_renderer;
    std::unique_ptr<ResourceManager> m_resourceManager;
    std::unique_ptr<ComponentRegister> m_componentRegister;
};
