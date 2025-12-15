#include "Hierarchy.h"

#include <imgui.h>
#include <ranges>

#include "Core/Engine.h"

#include "Scene/GameObject.h"
#include "Scene/Scene.h"

Hierarchy::Hierarchy(Engine* engine): EditorWindow(engine)
{
    m_sceneHolder = engine->GetSceneHolder();
}

void Hierarchy::OnRender()
{
    Scene* scene = m_sceneHolder->GetCurrentScene();
    GameObjectList gameObjects = scene->GetGameObjects();
    
    if (ImGui::Begin("Hierarchy"))
    {
        for (const auto& [uuid, gameObject] : gameObjects)
        {
            ImGui::PushID(uuid);
            
            std::string name = gameObject->GetName();
            if (ImGui::Selectable(name.c_str()))
            {
                EOnObjectSelected.Invoke(uuid);
            }
            
            ImGui::PopID();
        }
        if (ImGui::Button("Create Empty"))
        {
            scene->CreateGameObject();
        }
    }
    ImGui::End();
}
