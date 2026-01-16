#pragma once
#include "EditorWindow.h"
#include "Resource/IResource.h"

class ResourcesWindow : public EditorWindow
{
public:
    using EditorWindow::EditorWindow;
    
    void OnRender() override;
    
private:
    ResourceType m_resourceTypeFilter = ResourceType::None; 
};
