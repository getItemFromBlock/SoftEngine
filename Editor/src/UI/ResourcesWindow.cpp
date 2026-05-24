#include "ResourcesWindow.h"

#include "Inspector.h"
#include "Core/Engine.h"

void ResourcesWindow::OnRender()
{
    if (ImGui::Begin("Resources"))
    {
        auto engine = Engine::Get();
        ImGui::Text("FPS: %f", ImGui::GetIO().Framerate);
        ImGui::Text("Thread Pool usage: %.2f%%", ThreadPool::GetUsage() * 100.0f);
        ImGui::Text("Triangle Count: %llu", engine->GetRenderer()->GetTriangleCount());
        ImGui::Text("Vertex Count: %llu", engine->GetRenderer()->GetVertexCount());
        ImGui::Text("Softbody Chunk Count: %llu", engine->GetRenderer()->GetChunkCount());
        ImGui::Text("Softbody Particles Count: %llu", engine->GetRenderer()->GetParticleCount());
        ImGui::Text("Softbody Connections Count: %llu", engine->GetRenderer()->GetConnectionCount());

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

            constexpr ImGuiTableFlags tableFlags =
                ImGuiTableFlags_BordersOuter |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_SizingStretchProp;

            if (ImGui::BeginTable("ResourceTable", 5, tableFlags, ImVec2(0, 0)))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Type",  ImGuiTableColumnFlags_WidthFixed, 100.f);
                ImGui::TableSetupColumn("UUID",  ImGuiTableColumnFlags_WidthFixed, 220.f);
                ImGui::TableSetupColumn("Path",  ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.f);
                ImGui::TableHeadersRow();

                for (const auto& pair : engine->GetResourceManager()->GetResources())
                {
                    ImGui::PushID(pair.first);
                    auto resource = pair.second;

                    if (!resource)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("Invalid resource");
                        ImGui::PopID();
                        continue;
                    }

                    if (m_resourceTypeFilter != ResourceType::None &&
                        resource->GetResourceType() != m_resourceTypeFilter)
                    {
                        ImGui::PopID();
                        continue;
                    }

                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(resource->GetName().c_str());

                    // Right-click context menu on the entire row
                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                        ImGui::OpenPopup("##resource_ctx");

                    if (ImGui::BeginPopup("##resource_ctx"))
                    {
                        ImGui::TextDisabled("%s", resource->GetName().c_str());
                        ImGui::Separator();
                        if (ImGui::MenuItem("View Details"))
                            m_selectedResource = resource;
                        if (ImGui::MenuItem("Copy UUID"))
                            ImGui::SetClipboardText(std::to_string(resource->GetUUID()).c_str());
                        if (ImGui::MenuItem("Copy Path"))
                            ImGui::SetClipboardText(resource->GetPath().generic_string().c_str());
                        ImGui::EndPopup();
                    }

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(to_string(resource->GetResourceType()));

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(std::to_string(resource->GetUUID()).c_str());

                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(resource->GetPath().generic_string().c_str());

                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(to_string(resource->GetState()));

                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }

        // Details popup — opened when m_selectedResource is set
        if (m_selectedResource)
            ImGui::OpenPopup("Resource Details");

        ImGui::SetNextWindowSize(ImVec2(520, 400), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("Resource Details", nullptr, ImGuiWindowFlags_NoResize))
        {
            if (m_selectedResource)
            {
                // Header
                ImGui::TextUnformatted(m_selectedResource->GetName().c_str());
                ImGui::SameLine();
                ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(to_string(m_selectedResource->GetResourceType())).x);
                ImGui::TextDisabled("%s", to_string(m_selectedResource->GetResourceType()));
                ImGui::Separator();
                ImGui::Spacing();

                // Property grid
                if (ImGui::BeginTable("##detailprops", 2,
                    ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
                {
                    ImGui::TableSetupColumn("##key",   ImGuiTableColumnFlags_WidthFixed, 80.f);
                    ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch);

                    auto Row = [](const char* label, const std::string& value)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextDisabled("%s", label);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextUnformatted(value.c_str());
                    };

                    Row("UUID",  std::to_string(m_selectedResource->GetUUID()));
                    Row("Type",  to_string(m_selectedResource->GetResourceType()));
                    Row("Path",  m_selectedResource->GetPath().generic_string());
                    Row("State", to_string(m_selectedResource->GetState()));

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Descriptor fields — scrollable region so they don't overflow
                if (ImGui::BeginChild("##descriptor_scroll", ImVec2(0, -40), ImGuiChildFlags_None))
                {
                    ClassDescriptor descriptor;
                    m_selectedResource->Describe(descriptor);
                    Inspector::ShowDescriptor(descriptor);
                }
                ImGui::EndChild();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            float buttonWidth = 100.f;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x - buttonWidth);
            if (ImGui::Button("Close", ImVec2(buttonWidth, 0)))
            {
                m_selectedResource = nullptr;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }
    ImGui::End();
}