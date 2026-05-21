#include "Hierarchy.h"

#include <imgui.h>
#include <ranges>

#include "Core/Engine.h"
#include "Core/ImGuiHandler.h"

#include "Scene/GameObject.h"
#include "Scene/Scene.h"

Hierarchy::Hierarchy(Engine* engine, ImGuiHandler* handler): EditorWindow(handler)
{
    m_sceneHolder = engine->GetSceneHolder();
    EOnObjectRightClicked.Bind(std::bind(&Hierarchy::SetRightClickedObject, this, std::placeholders::_1));
}

static std::unordered_map<Core::UUID, bool> openMap;
static std::unordered_map<Core::UUID, bool> selectedMap;
void Hierarchy::DisplayObject(GameObject* object, uint64_t& index, bool display)
{
    auto children = object->GetChildren();
    if (!display)
    {
        // Do the same for all children
        for (auto&& child : children)
        {
            if (!child)
                continue;
            ImGui::TreePush(child->GetName().c_str());
            index++;
            DisplayObject(child.getPtr(), index, display);
            ImGui::TreePop();
        }
        return;
    }
    ImGui::PushID(static_cast<int>(index));
    // Display arrow button
    if (!object->GetChildrenUUID().empty())
    {
        auto& open = openMap[object->GetUUID()];
        if (!open)
        {
            if (ImGui::ArrowButton("##right", ImGuiDir_Right))
            {
                open = true;
            }
        }
        else if (open)
        {
            if (ImGui::ArrowButton("##down", ImGuiDir_Down))
            {
                open = false;
            }
        }
        ImGui::SameLine(0, 10);
    }
    else
    {
        ImGui::Button("O", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()  ));
        ImGui::SameLine();
    }

    // Rename Field
    bool& selected = selectedMap[object->GetUUID()];
    if (m_renameObject == object->GetUUID())
    {
        auto renameObject = object->GetScene()->GetGameObject(m_renameObject);
        static std::string name;
        name = renameObject->GetName();
        if (m_openRename)
        {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::InputText("##InputText", &name);
        if (m_renameObject && !m_openRename && !ImGui::IsItemActive())
        {
            renameObject->SetName(name);
            m_renameObject = UUID_INVALID;
        }
        m_openRename = false;
    }
    // Selectable Field
    else if (ImGui::Selectable(object->GetName().c_str(), &selected, ImGuiSelectableFlags_SelectOnNav))
    {
        for (auto& sel : selectedMap)
        {
            if (sel.first == object->GetUUID())
                continue;
            sel.second = false;
        }
        EOnObjectSelected.Invoke(selected ? object->GetUUID() : Core::UUID(-1));
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        EOnObjectRightClicked.Invoke(object->GetUUID());
    }
    
    if (display)
        display = openMap[object->GetUUID()];
    const auto drawList = ImGui::GetWindowDrawList();
    const float centerX = std::floor(ImGui::GetFrameHeight() * 0.5f);
    const ImVec2 verticalLine = ImGui::GetCursorScreenPos() + ImVec2(centerX, -4);
    ImVec2 lastChildPos;
    constexpr ImU32 white = IM_COL32_WHITE;
    // Do the same for all children
    for (size_t i = 0; i < children.size(); i++)
    {
        auto child = children[i];
        if (!child)
            continue;
        ImVec2 cursorPos = ImGui::GetCursorScreenPos() + ImVec2(centerX + 1, centerX);
        if (i == children.size() - 1)
            lastChildPos = ImGui::GetCursorScreenPos();
        
        // Draw horizontal line
        if (display)
            drawList->AddLine(cursorPos, cursorPos + ImVec2(centerX, 0), white);
        
        ImGui::TreePush(children[i]->GetName().c_str());
        
        index++;
        DisplayObject(child.getPtr(), index, display);
        
        ImGui::TreePop();
    }        
    
    if (!children.empty() && openMap[object->GetUUID()])
    {
        drawList->AddLine(verticalLine, lastChildPos + ImVec2(centerX, centerX + 1), white);
    }
    
    ImGui::PopID();
}

void Hierarchy::DisplayRightClickMenu()
{
    if (m_openRightClick)
    {
        ImGui::OpenPopup("Right Click");
        m_openRightClick = false;
    }
    if (ImGui::BeginPopup("Right Click"))
    {
        GameObject* gameObject = m_rightClickedObject == UUID_INVALID ? nullptr : m_sceneHolder->GetCurrentScene()->GetGameObject(m_rightClickedObject).getPtr();
        Scene* scene = gameObject ? gameObject->GetScene() : Engine::Get()->GetSceneHolder()->GetCurrentScene();
        if (ImGui::MenuItem("Create Empty"))
        {
            scene->CreateGameObject(gameObject);
            m_rightClickedObject = UUID_INVALID;
            ImGui::CloseCurrentPopup();
        }
        if (gameObject && ImGui::MenuItem("Delete"))
        {
            scene->DestroyGameObject(gameObject);
            m_rightClickedObject = UUID_INVALID;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Hierarchy::SetRightClickedObject(const Core::UUID& uuid)
{
    m_rightClickedObject = uuid;
    m_openRightClick = true;
}


void Hierarchy::OnRender()
{
    Scene* scene = m_sceneHolder->GetCurrentScene();
    GameObjectList gameObjects = scene->GetGameObjects();
    
    if (ImGui::Begin("Hierarchy"))
    {
        uint64_t index = 0;
        DisplayObject(scene->GetRootObject().getPtr(), index);
        if (!m_openRightClick && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && ImGui::IsWindowHovered())
        {
            m_rightClickedObject = UUID_INVALID;
            m_openRightClick = true;
        }
        DisplayRightClickMenu();
    }
    ImGui::End();
}

