#include "ResourcesWindow.h"

#include "Core/Engine.h"

void ResourcesWindow::OnRender()
{
    if (ImGui::Begin("Resources"))
    {
        ImGui::Text("FPS: %f", ImGui::GetIO().Framerate);
        ImGui::Text("Triangle Count: %llu", p_engine->GetRenderer()->GetTriangleCount());
        ImGui::Text("Vertex Count: %llu", p_engine->GetRenderer()->GetVertexCount());
        
        if (ImGui::CollapsingHeader("Resources"))
        {
            if (ImGui::BeginCombo("Resource Type", to_string(m_resourceTypeFilter)))
            {
                for (size_t i = 0; i < static_cast<int>(ResourceType::Count); i++)
                {
                    ResourceType resource = static_cast<ResourceType>(i);
                    bool is_selected = (m_resourceTypeFilter == resource);
                    if (ImGui::Selectable(to_string(resource), is_selected))
                        m_resourceTypeFilter = resource;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();  
                }
                ImGui::EndCombo();
            }
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
                if (m_resourceTypeFilter != ResourceType::None && resource->GetResourceType() != m_resourceTypeFilter)
                {
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
