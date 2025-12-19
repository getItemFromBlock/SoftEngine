#pragma once
#include <imgui.h>

class ImGuiHandler;
class Engine;

class EditorWindow
{
public:
    EditorWindow(Engine* engine, ImGuiHandler* imguiHandler) : p_engine(engine), m_imguiHandler(imguiHandler) {}
    virtual ~EditorWindow() = default;
    
    virtual void OnRender() = 0;
protected:
    Engine* p_engine;
    ImGuiHandler* m_imguiHandler = nullptr;
};
