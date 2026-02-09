#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

class ImGuiHandler;
class Engine;

class EditorWindow
{
public:
    EditorWindow(Engine* engine, ImGuiHandler* imguiHandler) : m_imguiHandler(imguiHandler) {}
    virtual ~EditorWindow() = default;
    
    virtual void OnRender() = 0;
protected:
    ImGuiHandler* m_imguiHandler = nullptr;
};
