#include "EditorWindowManager.h"

#include "Hierarchy.h"
#include "Inspector.h"
#include "ResourcesWindow.h"
#include "ViewportWindow.h"
#include "Core/Engine.h"

void EditorWindowManager::Initialize(Engine* engine, ImGuiHandler* handler)
{
    m_engine = engine;

    auto hierarchy = std::make_unique<Hierarchy>(engine, handler);
    auto inspector = std::make_unique<Inspector>(engine, handler);

    Inspector* inspectorPtr = inspector.get();
    
    hierarchy->EOnObjectSelected.Bind([inspectorPtr](const Core::UUID& uuid)
    {
        inspectorPtr->SetSelectedObject(uuid);
    });
    
    m_windows.push_back(std::move(hierarchy));
    m_windows.push_back(std::move(inspector));
    m_windows.push_back(std::make_unique<ResourcesWindow>(handler));
    m_windows.push_back(std::make_unique<ViewportWindow>(handler, engine->GetSceneHolder()->GetCurrentScene()->GetEditorCamera()));
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
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        window_flags |= ImGuiWindowFlags_NoBackground;
    ImGui::GetWindowDockID();

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleColor();

    ImGui::PopStyleVar(3);

    const ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        const ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    }
    ImGui::End();
}

void EditorWindowManager::RenderMainBar() const
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View"))
        {
            for (auto& window : m_windows)
            {
                if (ImGui::MenuItem(window->GetTitle().c_str(), nullptr, window->IsOpened()))
                {
                    window->SetOpened(!window->IsOpened());
                }
            }
            ImGui::EndMenu();
        }

        if (m_engine)
        {
            using RuntimeMode = Engine::RuntimeMode;
            RuntimeMode mode = m_engine->GetRuntimeMode();

            constexpr float buttonWidth = 70.f;
            constexpr float spacing = 6.f;
            const float controlsWidth = buttonWidth * 3.f + spacing * 2.f;
            const float startX = (ImGui::GetWindowWidth() - controlsWidth) * 0.5f;
            ImGui::SetCursorPosX(startX);

            const char* playLabel = mode == RuntimeMode::Pause ? "Resume" : "Play";
            ImGui::BeginDisabled(mode == RuntimeMode::Play);
            if (ImGui::Button(playLabel, ImVec2(buttonWidth, 0)))
            {
                m_engine->SetRuntimeMode(RuntimeMode::Play);
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, spacing);
            ImGui::BeginDisabled(mode != RuntimeMode::Play);
            if (ImGui::Button("Pause", ImVec2(buttonWidth, 0)))
            {
                m_engine->SetRuntimeMode(RuntimeMode::Pause);
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, spacing);
            ImGui::BeginDisabled(mode == RuntimeMode::Edit);
            if (ImGui::Button("Stop", ImVec2(buttonWidth, 0)))
            {
                m_engine->SetRuntimeMode(RuntimeMode::Edit);
            }
            ImGui::EndDisabled();
        }

        ImGui::EndMainMenuBar();
    }
}

void EditorWindowManager::Render() const
{
    RenderMainDock();
    RenderMainBar();
    for (auto& window : m_windows)
        window->OnRender();
}
