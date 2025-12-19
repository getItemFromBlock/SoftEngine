#pragma once
#include <memory>
#include "Core/Window.h"

#include "ImGuiHandler.h"
#include "UI/EditorWindowManager.h"

class Engine;

class Editor
{
public:
    Editor();
    
    static Editor* Create();
    static Editor* Get() { return s_instance.get(); }
    
    void Initialize();
    void Run();
    void OnRender();
    void Cleanup();
private:
    inline static std::unique_ptr<Editor> s_instance = nullptr;
    
    Engine* m_engine;
    std::unique_ptr<ImGuiHandler> m_imguiHandler;
    std::unique_ptr<Window> m_window = nullptr;
    std::unique_ptr<EditorWindowManager> m_windowManager = nullptr;
};
