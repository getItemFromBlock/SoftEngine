#pragma once
#include <memory>
#include <string>
#include <vector>

#include "EditorWindow.h"

class ImGuiHandler;

class EditorWindowManager
{
public:
    void Initialize(Engine* engine, ImGuiHandler* handler);

    void Render();
private:
    static void RenderMainDock();
    
    void RenderMainBar();
private:
    std::vector<std::unique_ptr<EditorWindow>> m_windows;
    Engine* m_engine = nullptr;
    std::string m_scenePathBuffer;
    
};
