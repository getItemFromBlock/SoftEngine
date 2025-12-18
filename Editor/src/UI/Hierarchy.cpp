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

void Hierarchy::DisplayObject(GameObject* object)
{
    if (!object)
        return;
    
    ImGui::PushID(object->GetUUID());
    bool open = ImGui::TreeNode(object->GetName().c_str());
    if (ImGui::IsItemHovered() && ImGui::IsItemClicked(ImGuiMouseButton_Left))
    {
        EOnObjectSelected.Invoke(object->GetUUID());
    }
    if (open)
    {
        for (auto& child : object->GetChildren())
        {
            DisplayObject(child.getPtr());
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}


void Hierarchy::OnRender()
{
    Scene* scene = m_sceneHolder->GetCurrentScene();
    GameObjectList gameObjects = scene->GetGameObjects();
    
    if (ImGui::Begin("Hierarchy"))
    {
        DisplayObject(scene->GetRootObject().getPtr());
    }
    ImGui::End();
}