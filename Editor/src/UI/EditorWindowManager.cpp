#include "EditorWindowManager.h"

#include "Hierarchy.h"
#include "Inspector.h"

void EditorWindowManager::Initialize(Engine* engine)
{
    m_windows.push_back(std::make_unique<Hierarchy>(engine));
    m_windows.push_back(std::make_unique<Inspector>(engine));
}

void EditorWindowManager::Render() const
{
    for (auto& window : m_windows)
        window->OnRender();
}
