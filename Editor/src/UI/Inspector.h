#pragma once
#include "EditorWindow.h"
#include "Core/UUID.h"

class Inspector : public EditorWindow
{
public:
    Inspector(Engine* engine);
    
    void OnRender() override;
    
    void SetSelectedObject(Core::UUID uuid);
private:
    Core::UUID m_selectedObject;
};
