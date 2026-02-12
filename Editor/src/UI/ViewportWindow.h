#pragma once
#include "EditorWindow.h"

class Camera;

class ViewportWindow : public EditorWindow
{
public:
    ViewportWindow(ImGuiHandler* imguiHandler, Camera* camera);

    void OnRender() override;
    
private:
    Camera* m_camera;
};
