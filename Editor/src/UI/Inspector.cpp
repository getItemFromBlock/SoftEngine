#include "Inspector.h"

#include "Core/Engine.h"
#include "Scene/GameObject.h"
#include "Scene/Scene.h"

Inspector::Inspector(Engine* engine) : EditorWindow(engine)
{
    
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
        
        Scene* scene = p_engine->GetSceneHolder()->GetCurrentScene();

        if (SafePtr<GameObject> object = scene->GetGameObject(m_selectedObject))
        {
            ImGui::Text("Name: %s", object->GetName().c_str());
        }
    }
    ImGui::End();
}

void Inspector::SetSelectedObject(Core::UUID uuid)
{
    m_selectedObject = uuid;
}
