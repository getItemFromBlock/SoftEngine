#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

class ImGuiHandler;
class Engine;

class EditorWindow
{
public:
    EditorWindow(const char* title, ImGuiHandler* imguiHandler) : p_imguiHandler(imguiHandler), p_title(title) {}
    virtual ~EditorWindow() = default;
    
    virtual void OnRender() = 0;
    
    std::string GetTitle() const { return p_title; }
    
    virtual void SetOpened(bool opened) { p_opened = opened; }
    virtual bool IsOpened() const { return p_opened; }
protected:
    ImGuiHandler* p_imguiHandler = nullptr;
    
    std::string p_title;
    bool p_opened = true;
};
