#pragma once
#include "EditorWindow.h"
#include "Utils/Event.h"

class Camera;

class ViewportWindow : public EditorWindow
{
public:
    ViewportWindow(ImGuiHandler* imguiHandler, Camera* camera);

    void RenderMenuBar() const;
    void OnRender() override;
    
    void SetCamera(Camera* camera);
private:
    Camera* m_camera;
    
    EventHandle m_renderTargetResizedHandle;
    EventHandle m_destroyHandle;
};
