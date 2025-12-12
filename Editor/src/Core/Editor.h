#pragma once
#include <memory>

#include "ImGuiHandler.h"

class Engine;

class Editor
{
public:
    Editor();
    
    void Initialize();
    void Run();
    void OnRender();
    void Cleanup();
private:
    Engine* m_engine;
    std::unique_ptr<ImGuiHandler> m_imguiHandler;
    
};
