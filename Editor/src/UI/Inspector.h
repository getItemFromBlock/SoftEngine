#pragma once
#include "EditorWindow.h"
#include "Core/UUID.h"
#include "Utils/Type.h"

class SceneHolder;

class Inspector : public EditorWindow
{
public:
    Inspector(Engine* engine);
    
    void OnRender() override;
    
    void SetSelectedObject(Core::UUID uuid);

private:
    SceneHolder* m_sceneHolder;
    
    Core::UUID m_selectedObject;
};
