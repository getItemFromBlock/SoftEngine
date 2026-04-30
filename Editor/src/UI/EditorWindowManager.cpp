#include "EditorWindowManager.h"

#include "Hierarchy.h"
#include "Inspector.h"
#include "ResourcesWindow.h"
#include "ViewportWindow.h"
#include "Component/LightComponent.h"
#include "Core/Engine.h"
#include "Utils/Platform.h"

void EditorWindowManager::Initialize(Engine* engine, ImGuiHandler* handler)
{
    m_engine = engine;
    m_scenePathBuffer = engine->GetSceneHolder()->GetScenePath().generic_string();

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

void EditorWindowManager::RenderMainBar()
{
    bool openModelPopup = false;
	const std::vector filters = { Platform::Filter("Scene", "scene") };
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Scene"))
            {
                m_engine->GetSceneHolder()->NewScene();
            }
            if (ImGui::MenuItem("Save Scene"))
            {
                if (const std::string path = Platform::SaveDialog(filters, m_scenePathBuffer); !path.empty())
                {
                    if (!m_engine->GetSceneHolder()->SaveCurrentScene(path))
                    {
                        PrintError("Failed to save scene %s", path.c_str());
                        m_scenePathBuffer = m_engine->GetSceneHolder()->GetScenePath().generic_string();
                    }
                }
            }
            
            if (ImGui::MenuItem("Load Scene"))
            {
                if (const std::string path = Platform::OpenDialog(filters, m_scenePathBuffer); !path.empty())
                {
                    if (!m_engine->GetSceneHolder()->LoadScene(path))
                    {
                        PrintError("Failed to load scene %s", path.c_str());
                        m_scenePathBuffer = m_engine->GetSceneHolder()->GetScenePath().generic_string();
                    }
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
            {
                Engine::Get()->GetWindow()->Close(true);
            }

            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("GameObject"))
        {
            if (ImGui::MenuItem("From Model"))
            {
                openModelPopup = true;
            }
            if (ImGui::BeginMenu("Light"))
            {
                if (ImGui::MenuItem("Directional"))
                {
                    auto currentScene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
                    auto light = currentScene->CreateGameObject();
                    light->SetName("Directional");
                    auto lightComponent = light->AddComponent<LightComponent>();
                    lightComponent->SetLightType(0);
                }
                if (ImGui::MenuItem("Point"))
                {
                    auto currentScene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
                    auto light = currentScene->CreateGameObject();
                    light->SetName("Point");
                    auto lightComponent = light->AddComponent<LightComponent>();
                    lightComponent->SetLightType(1);
                }
                if (ImGui::MenuItem("Spot"))
                {
                    auto currentScene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
                    auto light = currentScene->CreateGameObject();
                    light->SetName("Spot");
                    auto lightComponent = light->AddComponent<LightComponent>();
                    lightComponent->SetLightType(2);
                }
                if (ImGui::MenuItem("Line"))
                {
                    auto currentScene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
                    auto light = currentScene->CreateGameObject();
                    light->SetName("Line");
                    auto lightComponent = light->AddComponent<LightComponent>();
                    lightComponent->SetLightType(3);
                }
                
                ImGui::EndMenu();
            }
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
    if (openModelPopup)
    {
        ImGui::OpenPopup("Resource Popup");
    }
    if (auto output = Inspector::DisplayResourcePopup<Model>(); output.has_value() && output.value())
    {
        output.value()->EOnLoaded += [output]
        {
            auto currentScene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
            Model::CreateGameObject(output.value().getPtr(), currentScene);
        };
    }
}

void EditorWindowManager::Render()
{
    RenderMainDock();
    RenderMainBar();
    for (auto& window : m_windows)
        window->OnRender();
}
