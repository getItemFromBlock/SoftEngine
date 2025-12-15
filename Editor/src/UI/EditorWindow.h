#pragma once

#include <imgui.h>

class Engine;

class EditorWindow
{
public:
    EditorWindow(Engine* engine) : p_engine(engine) {}
    
    virtual void OnRender() = 0;
protected:
    Engine* p_engine;
};
