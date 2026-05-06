#pragma once
#include "EditorWindow.h"
#include "Resource/IResource.h"

class ResourcesWindow : public EditorWindow
{
public:
    ResourcesWindow(ImGuiHandler* handler) : EditorWindow("ResourcesWindow", handler) {}
    
    void OnRender() override;
    
private:
    ResourceType m_resourceTypeFilter = ResourceType::None; 
    SafePtr<IResource> m_selectedResource;
};
