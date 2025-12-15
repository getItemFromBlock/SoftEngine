#include "Inspector.h"

#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Engine.h"
#include "Scene/GameObject.h"
#include "Scene/Scene.h"

Inspector::Inspector(Engine* engine) : EditorWindow(engine)
{
    m_sceneHolder = engine->GetSceneHolder();
}

void Inspector::OnRender()
{
    if (ImGui::Begin("Inspector"))
    {
        if (m_selectedObject == UUID_INVALID)
        {
            ImGui::End();
            return;
        }
        
        Scene* scene = m_sceneHolder->GetCurrentScene();

        if (SafePtr<GameObject> object = scene->GetGameObject(m_selectedObject))
        {
            ImGui::Text("Name: %s", object->GetName().c_str());

            auto components = object->GetComponents();
            for (auto& component : components)
            {
                // ImGui::Text("Component: %s", component->GetTypeName().c_str());
                if (ImGui::CollapsingHeader(component->GetTypeName().c_str()))
                {
                }
            }
        }
    }
    ImGui::End();
}

void Inspector::SetSelectedObject(Core::UUID uuid)
{
    m_selectedObject = uuid;
}
