#include "EditorWindowManager.h"

#include "Hierarchy.h"
#include "Inspector.h"

void EditorWindowManager::Initialize(Engine* engine)
{
    auto hierarchy = std::make_unique<Hierarchy>(engine);
    auto inspector = std::make_unique<Inspector>(engine);

    Hierarchy* hierarchyPtr = hierarchy.get();
    Inspector* inspectorPtr = inspector.get();
    
    hierarchy->EOnObjectSelected += [inspectorPtr](const Core::UUID& uuid)
    {
        inspectorPtr->SetSelectedObject(uuid);
    };
    
    m_windows.push_back(std::move(hierarchy));
    m_windows.push_back(std::move(inspector));
}

void EditorWindowManager::RenderMainDock()
{
    static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;
    ImGui::GetWindowDockID();

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleColor();

    ImGui::PopStyleVar(2);

    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        const ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    ImGui::End();
}

void EditorWindowManager::Render() const
{
    // RenderMainDock();
    for (auto& window : m_windows)
        window->OnRender();
}
