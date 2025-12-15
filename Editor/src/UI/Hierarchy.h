#pragma once
#include "EditorWindow.h"
#include "Core/UUID.h"
#include "Utils/Event.h"

class SceneHolder;

class Hierarchy : public EditorWindow
{
public:
    Hierarchy(Engine* engine);

    void OnRender() override;
    
    Event<Core::UUID> EOnObjectSelected;
private:
    SceneHolder* m_sceneHolder;
    
    Core::UUID m_selectedObject = UUID_INVALID;
};
