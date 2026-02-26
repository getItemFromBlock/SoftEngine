#include "ViewportWindow.h"

#include "Inspector.h"
#include "Core/Engine.h"
#include "Core/ImGuiHandler.h"
#include "Resource/RenderTargetTexture.h"

ViewportWindow::ViewportWindow(ImGuiHandler* imguiHandler, Camera* camera) : EditorWindow(imguiHandler),
                                                                             m_camera(camera)
{
    m_camera->OnRenderTargetResized += [this](const Vec2i& size)
    {
        m_imguiHandler->UpdateTextureID(m_camera->GetRenderTarget().getPtr());
    };
}

void ViewportWindow::RenderMenuBar() const
{
    if (ImGui::BeginMenuBar())
    {
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(0, 0),
            ImVec2(300.0f, FLT_MAX)
        );

        if (ImGui::BeginMenu("Camera"))
        {
            ClassDescriptor descriptor;
            m_camera->Describe(descriptor);
            Inspector::ShowDescriptor(descriptor);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
}

void ViewportWindow::OnRender()
{
    if (ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_MenuBar))
    {
        RenderMenuBar();

        const float contentWidth = ImGui::GetContentRegionAvail().x;
        const float contentHeight = ImGui::GetContentRegionAvail().y;

        constexpr bool keepAspectRatio = true;

        float width = contentWidth, height = contentHeight;

        if (keepAspectRatio)
        {
            constexpr float aspectRatio = 16.f / 9.f;

            // Calculate dimensions while maintaining the aspect ratio
            if (contentWidth / aspectRatio <= contentHeight)
            {
                width = contentWidth;
                height = width / aspectRatio;
            }
            else
            {
                height = contentHeight;
                width = height * aspectRatio;
            }
        }
        const float xPos = (ImGui::GetContentRegionAvail().x - width) * 0.5f;
        const float yPos = (ImGui::GetContentRegionAvail().y - height) * 0.5f;

        const auto cursorPos = ImGui::GetCursorPos();

        ImGui::SetCursorPos(ImVec2(cursorPos.x + xPos, cursorPos.y + yPos));

        ImTextureRef textureID = m_imguiHandler->GetTextureID(m_camera->GetRenderTarget().getPtr());

        ImGui::Image(textureID, ImVec2(int(width), int(height)));
        ImGui::SetCursorPos(ImVec2(cursorPos.x + xPos, cursorPos.y + yPos));

        auto renderTargetSize = m_camera->GetRenderTargetSize();

        if (renderTargetSize != Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height)))
        {
            m_camera->SetRenderTargetSize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
    }
    ImGui::End();
}
