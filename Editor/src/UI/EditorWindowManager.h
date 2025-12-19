#pragma once
#include <memory>
#include <vector>

#include "EditorWindow.h"

class ImGuiHandler;

class EditorWindowManager
{
public:
    void Initialize(Engine* engine, ImGuiHandler* handler);

    void Render() const;
private:
    static void RenderMainDock();
private:
    std::vector<std::unique_ptr<EditorWindow>> m_windows;
    
};
