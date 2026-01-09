#include "ResourcesWindow.h"

#include "Core/Engine.h"

void ResourcesWindow::OnRender()
{
    if (ImGui::Begin("Resources"))
    {
        ImGui::Text("FPS: %f", ImGui::GetIO().Framerate);
        ImGui::Text("Triangle Count: %llu", p_engine->GetRenderer()->GetTriangleCount());
        
        if (ImGui::CollapsingHeader("Resources"))
        {
            for (const auto& pair : p_engine->GetResourceManager()->GetResources())
            {
                ImGui::PushID(pair.first);
                auto resource = pair.second;
                if (!resource)
                {
                    ImGui::Text("Invalid resource");
                    ImGui::PopID();
                    continue;
                }
                if (ImGui::TreeNode(resource->GetName().c_str()))
                {
                    ImGui::BeginDisabled();
                    ImGui::Text("UUID: %llu", resource->GetUUID());
                    ImGui::Text("Type: %s", to_string(resource->GetResourceType()));
                    ImGui::Text("Path: %s", resource->GetPath().generic_string().c_str());
                    bool isLoaded = resource->IsLoaded();
                    ImGui::Checkbox("Loaded", &isLoaded);
                    bool sentToGpu = resource->SentToGPU();
                    ImGui::Checkbox("Sent to GPU", &sentToGpu);
                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::End();
}
