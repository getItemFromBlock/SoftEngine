#include "Inspector.h"

#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"

#include "Core/Engine.h"
#include "Core/ImGuiHandler.h"
#include "Resource/ResourceManager.h"

#include "Scene/GameObject.h"
#include "Scene/Scene.h"

#include "Resource/Mesh.h"

Inspector::Inspector(Engine* engine, ImGuiHandler* handler) : EditorWindow(engine, handler)
{
    m_sceneHolder = engine->GetSceneHolder();
}

void Inspector::OnRender()
{
    if (ImGui::Begin("Inspector"))
    {
        if (m_selectedObject == UUID_INVALID)
        {
            ImGui::End();
            return;
        }

        Scene* scene = m_sceneHolder->GetCurrentScene();

        if (SafePtr<GameObject> object = scene->GetGameObject(m_selectedObject))
        {
            ImGui::Text("Name: %s", object->GetName().c_str());

            auto components = object->GetComponents();
            for (SafePtr<IComponent>& component : components)
            {
                ImGui::PushID(component->GetUUID());

                bool enable = component->IsEnable();
                if (ImGui::Checkbox("##", &enable))
                {
                    component->SetEnable(enable);
                }
                ImGui::SameLine();

                bool destroy = true;
                const bool open = ImGui::CollapsingHeader(component->GetTypeName(), &destroy,
                                                          ImGuiTreeNodeFlags_AllowOverlap |
                                                          ImGuiTreeNodeFlags_DefaultOpen);

                if (open)
                {
                    ClassDescriptor descriptor;
                    component->Describe(descriptor);
                    ShowProperty(descriptor);
                }
                ImGui::PopID();
            }
        }
    }
    ImGui::End();
}

void Inspector::SetSelectedObject(Core::UUID uuid)
{
    m_selectedObject = uuid;
}

void Inspector::ShowMaterials(const Property& property) const
{
    auto materials = static_cast<std::vector<SafePtr<Material>>*>(property.data);
    auto materialList = *materials;
    size_t i = 0;
    for (SafePtr<Material>& material : materialList)
    {
        ImGui::PushID(material->GetUUID());
        ImGui::Text("Material %d", i++);
        if (ImGui::Button(material->GetName().c_str()))
        {
            ImGui::OpenPopup("Material Popup");
        }
        if (ImGui::BeginPopup("Material Popup"))
        {
            auto resourceManager = p_engine->GetResourceManager();
            auto allMaterials = resourceManager->GetAll<Material>();
            for (auto& mat : allMaterials)
            {
                ImGui::PushID(mat->GetUUID());
                if (ImGui::MenuItem(mat->GetName().c_str()))
                {
                    material = mat;
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        if (ImGui::TreeNode("Details"))
        {
            ClassDescriptor descriptor;
            material->Describe(descriptor);
            ShowProperty(descriptor);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    *materials = materialList;
}

void Inspector::ShowMesh(const Property& property) const
{
    SafePtr<Mesh>* meshPtr = static_cast<SafePtr<Mesh>*>(property.data);
    SafePtr<Mesh> mesh = *meshPtr;
    auto meshName = mesh->GetName();
    if (ImGui::Button(meshName.c_str()))
    {
        ImGui::OpenPopup("Mesh Popup");
    }
    if (ImGui::BeginPopup("Mesh Popup"))
    {
        auto resourceManager = p_engine->GetResourceManager();
        auto meshes = resourceManager->GetAll<Mesh>();
        for (auto& mesh : meshes)
        {
            ImGui::PushID(mesh->GetUUID());
            if (ImGui::MenuItem(mesh->GetName().c_str()))
            {
                *meshPtr = mesh;
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
}

void Inspector::ShowTransform(const Property& property) const
{
    auto transform = static_cast<TransformComponent*>(property.data);
    Vec3f position = transform->GetLocalPosition();
    Vec3f eulerRotation = transform->GetLocalRotation().ToEuler();
    Vec3f scale = transform->GetLocalScale();
    if (ImGui::DragFloat3("Position", &position.x, 0.1f))
    {
        transform->SetLocalPosition(position);
    }
    if (ImGui::DragFloat3("Rotation", &eulerRotation.x, 0.1f))
    {
        transform->SetLocalRotation(eulerRotation.ToQuaternion());
    }
    if (ImGui::DragFloat3("Scale", &scale.x, 0.1f))
    {
        transform->SetLocalScale(scale);
    }
}

void Inspector::ShowProperty(const ClassDescriptor& descriptor) const
{
    for (auto& property : descriptor.properties)
    {
        switch (property.type)
        {
        case PropertyType::None:
            ImGui::Text("None");
            break;
        case PropertyType::Bool:
            ImGui::Checkbox(property.name.c_str(), static_cast<bool*>(property.data));
            break;
        case PropertyType::Int:
            ImGui::InputInt(property.name.c_str(), static_cast<int*>(property.data));
            break;
        case PropertyType::Float:
            ImGui::DragFloat(property.name.c_str(), static_cast<float*>(property.data));
            break;
        case PropertyType::Vec2f:
            ImGui::DragFloat2(property.name.c_str(), &static_cast<Vec2f*>(property.data)->x);
            break;
        case PropertyType::Vec3f:
            ImGui::DragFloat3(property.name.c_str(), &static_cast<Vec3f*>(property.data)->x);
            break;
        case PropertyType::Vec4f:
            ImGui::DragFloat4(property.name.c_str(), &static_cast<Vec4f*>(property.data)->x);
            break;
        case PropertyType::Quat:
            {
                auto quat = static_cast<Quat*>(property.data);
                auto euler = quat->ToEuler();
                if (ImGui::DragFloat3(property.name.c_str(), &euler.x))
                {
                    *quat = Quat::FromEuler(euler);
                }
                break;
            }
        case PropertyType::Color3:
            ImGui::ColorEdit3(property.name.c_str(), &static_cast<Vec3f*>(property.data)->x);
            break;
        case PropertyType::Color4:
            ImGui::ColorEdit4(property.name.c_str(), &static_cast<Vec4f*>(property.data)->x);
            break;
        case PropertyType::Texture:
            {
                auto texturePtr = static_cast<SafePtr<Texture>*>(property.data);
                auto textureID = m_imguiHandler->GetTextureID(texturePtr->getPtr());
                ImGui::Image(textureID, ImVec2(64, 64));
                break;
            }
        case PropertyType::Mesh:
            ShowMesh(property);
            break;
        // case PropertyType::Material:
        case PropertyType::Materials:
            ShowMaterials(property);
            break;
        case PropertyType::Transform:
            ShowTransform(property);
            break;
        default:
            PrintError("Property type not handle on Inspector");
            break;
        }
    }
}
