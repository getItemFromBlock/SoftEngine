#include "Inspector.h"

#include "Component/MeshComponent.h"
#include "Component/ParticleSystemComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Editor.h"

#include "Core/Engine.h"
#include "Core/ImGuiHandler.h"
#include "Resource/CubeMap.h"
#include "Resource/ResourceManager.h"

#include "Scene/GameObject.h"
#include "Scene/Scene.h"

#include "Resource/Mesh.h"
#include "Resource/PostProcessShader.h"

Inspector::Inspector(Engine* engine, ImGuiHandler* handler) : EditorWindow(handler)
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
            size_t i = 0;
            Core::UUID deletedID = UUID_INVALID;
            for (SafePtr<IComponent>& component : components)
            {
                const ClassDescriptor& descriptor = GetDescriptor(component->GetUUID(), component);
                ImGui::PushID(component->GetUUID());

                bool enable = component->IsEnable();
                if (ImGui::Checkbox("##", &enable))
                {
                    component->SetEnable(enable);
                }
                ImGui::SameLine();

                bool visible = true;
                const bool open = ImGui::CollapsingHeader(component->GetTypeName(), &visible,
                                                          ImGuiTreeNodeFlags_AllowOverlap |
                                                          ImGuiTreeNodeFlags_DefaultOpen);

                if (!visible)
                {
                    deletedID = component->GetUUID();
                }
                if (open)
                {
                    ShowDescriptor(descriptor);
                }
                ImGui::PopID();
                i++;
            }
            if (deletedID != UUID_INVALID)
            {
                object->RemoveComponent(deletedID);
            }
        
            ImGui::NewLine();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, 2);
            ImGui::NewLine();
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("Add Component").x) * 0.5f);
            if (ImGui::Button("Add Component"))
            {
                ImGui::OpenPopup("Add Component");
            }
            DisplayAddComponentPopup();
        }
    }
    ImGui::End();
}

void Inspector::SetSelectedObject(const Core::UUID& uuid)
{
    m_selectedObject = uuid;

    if (SafePtr<GameObject> object = m_sceneHolder->GetCurrentScene()->GetGameObject(uuid))
    {
        std::vector<SafePtr<IComponent>> components = object->GetComponents();

        for (SafePtr<IComponent>& component : components)
        {
            ClassDescriptor descriptor;
            component->Describe(descriptor);
            m_descriptors[component->GetUUID()] = descriptor;
        }
    }
}

template <>
std::optional<SafePtr<Material>> Inspector::DisplayResourcePopup<Material>()
{
    std::optional<SafePtr<Material>> result;
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = Engine::Get()->GetResourceManager();
        auto materials = resourceManager->GetAll<Material>();
        if (ImGui::MenuItem("None"))
        {
            result = nullptr;
            ImGui::CloseCurrentPopup();
        }
        for (auto& material : materials)
        {
            ImGui::PushID(material->GetUUID());
            if (ImGui::MenuItem(material->GetName().c_str()))
            {
                result = resourceManager->Load<Material>(material->GetPath());
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    return result;
}

template <>
std::optional<SafePtr<Mesh>> Inspector::DisplayResourcePopup<Mesh>()
{
    std::optional<SafePtr<Mesh>> result;
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = Engine::Get()->GetResourceManager();
        auto meshes = resourceManager->GetAll<Mesh>();
        if (ImGui::MenuItem("None"))
        {
            result = nullptr;
            ImGui::CloseCurrentPopup();
        }
        for (auto& mesh : meshes)
        {
            ImGui::PushID(mesh->GetUUID());
            if (ImGui::MenuItem(mesh->GetName().c_str()))
            {
                result = resourceManager->Load<Mesh>(mesh->GetPath());
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    return result;
}

template <>
std::optional<SafePtr<Texture>> Inspector::DisplayResourcePopup<Texture>()
{
    std::optional<SafePtr<Texture>> result;
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = Engine::Get()->GetResourceManager();
        auto textures = resourceManager->GetAll<Texture>();
        if (ImGui::MenuItem("None"))
        {
            result = nullptr;
            ImGui::CloseCurrentPopup();
        }
        for (auto& texture : textures)
        {
            ImGui::PushID(texture->GetUUID());
            auto textureID = Editor::Get()->GetImGuiHandler()->GetTextureID(texture.get());
            if (ImGui::ImageButton(texture->GetPath().generic_string().c_str(), textureID, ImVec2(64, 64)))
            {
                result = resourceManager->Load<Texture>(texture->GetPath());
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            ImGui::Text(texture->GetName().c_str());
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    return result;
}

template <>
std::optional<SafePtr<CubeMap>> Inspector::DisplayResourcePopup<CubeMap>()
{
    std::optional<SafePtr<CubeMap>> result;
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = Engine::Get()->GetResourceManager();
        auto cubeMaps = resourceManager->GetAll<CubeMap>();
        if (ImGui::MenuItem("None"))
        {
            result = nullptr;
            ImGui::CloseCurrentPopup();
        }
        for (auto& cubeMap : cubeMaps)
        {
            ImGui::PushID(cubeMap->GetUUID());
            if (ImGui::MenuItem(cubeMap->GetName().c_str()))
            {
                result = resourceManager->Load<CubeMap>(cubeMap->GetPath());
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    return result;
}

template <>
std::optional<SafePtr<Shader>> Inspector::DisplayResourcePopup<Shader>()
{
    std::optional<SafePtr<Shader>> result;
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = Engine::Get()->GetResourceManager();
        auto shaders = resourceManager->GetAll<Shader>();
        if (ImGui::MenuItem("None"))
        {
            result = nullptr;
            ImGui::CloseCurrentPopup();
        }
        for (auto& shader : shaders)
        {
            ImGui::PushID(shader->GetUUID());
            if (ImGui::MenuItem(shader->GetName().c_str()))
            {
                result = resourceManager->Load<Shader>(shader->GetPath());
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    return result;
}

template <>
std::optional<SafePtr<PostProcessShader>> Inspector::DisplayResourcePopup<PostProcessShader>()
{
    std::optional<SafePtr<PostProcessShader>> result;
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = Engine::Get()->GetResourceManager();
        auto shaders = resourceManager->GetAll<PostProcessShader>();
        if (ImGui::MenuItem("None"))
        {
            result = nullptr;
            ImGui::CloseCurrentPopup();
        }
        for (auto& shader : shaders)
        {
            ImGui::PushID(shader->GetUUID());
            if (ImGui::MenuItem(shader->GetName().c_str()))
            {
                result = resourceManager->Load<PostProcessShader>(shader->GetPath());
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
    return result;
}

template <typename T>
bool DisplayWithType(const std::string& name, T* value)
{
    ImGui::TextUnformatted("Unknown");
    return false;
}

template <>
bool DisplayWithType(const std::string& name, float* value)
{
    return ImGui::DragFloat(name.c_str(), value);
}

template <>
bool DisplayWithType(const std::string& name, Vec4f* value)
{
    return ImGui::ColorEdit4(name.c_str(), &value->x);
}

template <typename T>
bool DisplayParticleValue(const std::string& name, ParticleProperty<T>& property)
{
    ImGui::PushID(name.c_str());
    ImGui::TextUnformatted(name.c_str());
    ImGui::SameLine();
    bool result = false;
    switch (property.type)
    {
    case ParticleProperty<T>::Type::Constant:
        {
            result |= DisplayWithType("##" + name, &property.value.min);
        }
        break;
    case ParticleProperty<T>::Type::Random:
        {
            float itemWidth = ImGui::GetContentRegionAvail().x * 0.5f - ImGui::GetFrameHeight() * 1.5f;
            ImGui::SetNextItemWidth(itemWidth);
            result |= DisplayWithType("##1" + name, &property.value.min);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(itemWidth);
            result |= DisplayWithType("##2" + name, &property.value.max);
        }
        break;
    default:
        break;
    }
    ImGui::SameLine();
    bool random = property.type == ParticleProperty<T>::Type::Random;
    if (ImGui::Checkbox("##Random", &random))
    {
        result = true;
        property.type = random ? ParticleProperty<T>::Type::Random : ParticleProperty<T>::Type::Constant;
    }
    ImGui::PopID();
    return result;
}

void Inspector::ShowDescriptor(const ClassDescriptor& descriptor)
{
    if (descriptor.properties.size() == 1 && descriptor.properties[0].type == PropertyType::ParticleSystem)
    {
        ShowParticleSystem(descriptor.properties[0]);
        return;
    }
    if (ImGui::BeginTable("PropertiesTable", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableSetupColumn("Property");
        ImGui::TableSetupColumn("Value");

        for (const auto& property : descriptor.properties)
        {
            if (property.isList)
            {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(property.name.c_str());
                ImGui::SameLine();
                if (ImGui::Button("+"))
                {
                    AddListElement(property);
                }
                ImGui::TableSetColumnIndex(1);
                size_t listSize = GetListSize(property);
                ImGui::Text("Size %d", listSize);

                if (listSize > 0)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
    
                    // Draw full-width separator using draw list
                    ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    float tableWidth = ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x;
                    ImVec2 tableStart = ImVec2(ImGui::GetCursorScreenPos().x - ImGui::GetStyle().ItemSpacing.x, cursorPos.y);
    
                    // Get table bounds properly
                    ImGuiTable* table = ImGui::GetCurrentTable();
                    if (table)
                    {
                        float x1 = table->OuterRect.Min.x;
                        float x2 = table->OuterRect.Max.x;
                        float y = cursorPos.y;
        
                        drawList->AddLine(ImVec2(x1, y), ImVec2(x2, y), ImGui::GetColorU32(ImGuiCol_Separator));
                        ImGui::Dummy(ImVec2(0, 1)); // Add vertical spacing
                    }
    
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::PushID(property.name.c_str());
                    RenderListProperty(property, "##" + property.name);
                    ImGui::PopID();
                }
            }
            else
            {
                ImGui::TableNextRow();

                // Label column
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(property.name.c_str());

                // Value column
                ImGui::TableSetColumnIndex(1);
                ShowProperty(property);
            }
        }

        ImGui::EndTable();
    }
}

void Inspector::ShowProperty(const Property& property)
{
    std::string id = ("##" + property.name);
    ImGui::PushID(id.c_str());

    ImGui::SetNextItemWidth(-FLT_MIN);
    switch (property.type)
    {
    case PropertyType::None:
        ImGui::Text("None");
        break;
    case PropertyType::Button:
    {
        RenderButtonProperty(property, id);
        break;
    }
    case PropertyType::Bool:
        {
            RenderBoolProperty(property, id);
            break;
        }
    case PropertyType::Int:
        {
            RenderIntProperty(property, id);
            break;
        }
    case PropertyType::Vec2i:
    {
        RenderIVec2Property(property, id);
        break;
    }
    case PropertyType::Vec3i:
    {
        RenderIVec3Property(property, id);
        break;
    }
    case PropertyType::Vec4i:
    {
        RenderIVec4Property(property, id);
        break;
    }
    case PropertyType::Float:
        {
            RenderFloatProperty(property, id);
            break;
        }
    case PropertyType::Vec2f:
        {
            RenderFVec2Property(property, id);
            break;
        }
    case PropertyType::Vec3f:
        {
            RenderFVec3Property(property, id);
            break;
        }
    case PropertyType::Vec4f:
        {
            RenderFVec4Property(property, id);
            break;
        }
    case PropertyType::Quat:
        {
            RenderQuatProperty(property, id);
            break;
        }
    case PropertyType::Color3:
        {
            RenderColor3Property(property, id);
            break;
        }
    case PropertyType::Color4:
        {
            RenderColor4Property(property, id);
            break;
        }
    case PropertyType::Enum:
    {
        RenderEnumProperty(property, id);
        break;
    }
    case PropertyType::Texture:
        {
            RenderTextureProperty(property, id);
            break;
        }
    case PropertyType::CubeMap:
        {
            RenderCubeMapProperty(property, id);
            break;
        }
    case PropertyType::Mesh:
        {
            RenderMeshProperty(property, id);
            break;
        }
    case PropertyType::Material:
        {
            RenderMaterialProperty(property, id);
            break;
        }
    case PropertyType::Shader:
        {
            RenderShaderProperty(property, id);
            break;
        }
    case PropertyType::PostProcessShader:
        {
            RenderPostProcessShaderProperty(property, id);
            break;
        }
    default:
        PrintWarning("Property type not handle on Inspector");
        break;
    }
    ImGui::PopID();
}

void Inspector::UpdateProperty(const Property& property, void* newValue)
{
    if (property.setter)
    {
        property.setter(newValue);
    }
    else if (property.data && !property.readOnly)
    {
        switch (property.type) // Memcpy only work for primitive types
        {
        case PropertyType::Texture:
            *static_cast<SafePtr<Texture>*>(property.data) = *static_cast<SafePtr<Texture>*>(newValue);
            break;
        case PropertyType::CubeMap:
            *static_cast<SafePtr<CubeMap>*>(property.data) = *static_cast<SafePtr<CubeMap>*>(newValue);
            break;
        case PropertyType::Mesh:
            *static_cast<SafePtr<Mesh>*>(property.data) = *static_cast<SafePtr<Mesh>*>(newValue);
            break;
        case PropertyType::Material:
            *static_cast<SafePtr<Material>*>(property.data) = *static_cast<SafePtr<Material>*>(newValue);
            break;
        default:
            size_t typeSize = GetPropertyTypeSize(property.type);
            if (typeSize > 0)
            {
                memcpy(property.data, newValue, typeSize);
            }
            break;
        }
    }

    // Mark as modified for undo/redo system
    if (property.onModified)
    {
        property.onModified();
    }
}

void Inspector::RenderButtonProperty(const Property& property, const std::string& id)
{
    if (ImGui::Button(id.c_str()))
    {
        UpdateProperty(property, nullptr);
    }
}

void Inspector::RenderBoolProperty(const Property& property, const std::string& id)
{
    bool value = *static_cast<const bool*>(property.data);
    if (ImGui::Checkbox(id.c_str(), &value))
    {
        UpdateProperty(property, &value);
    }
}

void Inspector::RenderIntProperty(const Property& property, const std::string& id)
{
    int value = *static_cast<const int*>(property.data);

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;
    if (property.readOnly)
        flags |= ImGuiInputTextFlags_ReadOnly;

    if (ImGui::InputInt(id.c_str(), &value, 1, 100, flags))
    {
        if (property.hasRange)
        {
            value = std::clamp(value, property.range.intRange.minInt, property.range.intRange.maxInt);
        }
        UpdateProperty(property, &value);
    }
}

void Inspector::RenderIVec2Property(const Property& property, const std::string& id)
{
    Vec2i value = *static_cast<const Vec2i*>(property.data);
    
    ImGuiSliderFlags flags = ImGuiSliderFlags_None;
    bool changed = false;

    if (property.hasRange)
        changed = ImGui::SliderInt2(id.c_str(), &value.x, property.range.intRange.minInt,
            property.range.intRange.maxInt);
    else
        changed = ImGui::DragInt2(id.c_str(), &value.x);

    if (changed)
        UpdateProperty(property, &value);
}

void Inspector::RenderIVec3Property(const Property& property, const std::string& id)
{
    Vec3i value = *static_cast<const Vec3i*>(property.data);
    
    ImGuiSliderFlags flags = ImGuiSliderFlags_None;
    bool changed = false;

    if (property.hasRange)
        changed = ImGui::SliderInt3(id.c_str(), &value.x, property.range.intRange.minInt,
            property.range.intRange.maxInt);
    else
        changed = ImGui::DragInt3(id.c_str(), &value.x);

    if (changed)
        UpdateProperty(property, &value);
}

void Inspector::RenderIVec4Property(const Property& property, const std::string& id)
{
    Vec4i value = *static_cast<const Vec4i*>(property.data);
    
    ImGuiSliderFlags flags = ImGuiSliderFlags_None;
    bool changed = false;

    if (property.hasRange)
        changed = ImGui::SliderInt4(id.c_str(), &value.x, property.range.intRange.minInt,
            property.range.intRange.maxInt);
    else
        changed = ImGui::DragInt4(id.c_str(), &value.x);

    if (changed)
        UpdateProperty(property, &value);
}

void Inspector::RenderFloatProperty(const Property& property, const std::string& id)
{
    float value = *static_cast<const float*>(property.data);

    ImGuiSliderFlags flags = ImGuiSliderFlags_None;
    bool changed = false;

    if (property.hasRange)
        changed = ImGui::SliderFloat(id.c_str(), &value, property.range.floatRange.minFloat,
                                     property.range.floatRange.maxFloat);
    else
        changed = ImGui::DragFloat(id.c_str(), &value, 0.01f);

    if (changed)
        UpdateProperty(property, &value);
}

void Inspector::RenderFVec2Property(const Property& property, const std::string& id)
{
    Vec2f value = *static_cast<const Vec2f*>(property.data);
    if (ImGui::DragFloat2(id.c_str(), &value.x, 0.01f))
    {
        UpdateProperty(property, &value);
    }
}

void Inspector::RenderFVec3Property(const Property& property, const std::string& id)
{
    Vec3f value = *static_cast<const Vec3f*>(property.data);
    if (ImGui::DragFloat3(id.c_str(), &value.x, 0.01f))
    {
        UpdateProperty(property, &value);
    }
}

void Inspector::RenderFVec4Property(const Property& property, const std::string& id)
{
    Vec4f value = *static_cast<const Vec4f*>(property.data);
    if (ImGui::DragFloat4(id.c_str(), &value.x, 0.01f))
    {
        UpdateProperty(property, &value);
    }
}

void Inspector::RenderQuatProperty(const Property& property, const std::string& id)
{
    const Quat* quatPtr = static_cast<const Quat*>(property.data);
    Vec3f euler = quatPtr->ToEuler();

    Vec3f eulerDeg = euler * (180.0f / PI);

    if (ImGui::DragFloat3(id.c_str(), &eulerDeg.x, 0.5f))
    {
        Vec3f eulerRad = eulerDeg * (PI / 180.0f);
        Quat newQuat = Quat::FromEuler(eulerRad);
        UpdateProperty(property, &newQuat);
    }

    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Quat(%.3f, %.3f, %.3f, %.3f)",
                          quatPtr->x, quatPtr->y, quatPtr->z, quatPtr->w);
    }
}

void Inspector::RenderColor3Property(const Property& property, const std::string& id)
{
    Vec3f value = *static_cast<const Vec3f*>(property.data);

    ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoAlpha |
        ImGuiColorEditFlags_DisplayRGB |
        ImGuiColorEditFlags_InputRGB |
        ImGuiColorEditFlags_PickerHueWheel;

    if (ImGui::ColorEdit3(id.c_str(), &value.x, flags))
    {
        UpdateProperty(property, &value);
    }
}

void Inspector::RenderColor4Property(const Property& property, const std::string& id)
{
    Vec4f value = *static_cast<const Vec4f*>(property.data);

    ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar |
        ImGuiColorEditFlags_DisplayRGB |
        ImGuiColorEditFlags_InputRGB |
        ImGuiColorEditFlags_PickerHueWheel;

    if (ImGui::ColorEdit4(id.c_str(), &value.x, flags))
    {
        UpdateProperty(property, &value);
    }
}

void Inspector::RenderEnumProperty(const Property & property, const std::string & id)
{
    int32_t *index = static_cast<int32_t *>(property.data);
    if (ImGui::Combo(property.name.c_str(), index, static_cast<const char*>(property.dataDescriptor)))
    {
        UpdateProperty(property, index);
    }
}

void Inspector::RenderTextureProperty(const Property& property, const std::string& id)
{
    SafePtr<Texture> texture = *static_cast<SafePtr<Texture>*>(property.data);

    ImTextureRef textureID;
    if (!texture.getPtr())
    {
        textureID = Editor::Get()->GetImGuiHandler()->GetTextureID(
            Engine::Get()->GetResourceManager()->GetDefaultTexture().get());
    }
    else
    {
        textureID = Editor::Get()->GetImGuiHandler()->GetTextureID(texture.getPtr());
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.f, 2.f));
    if (ImGui::ImageButton(id.c_str(), textureID, ImVec2(64, 64)))
    {
        ImGui::OpenPopup("Resource Popup");
    }
    
    auto result = DisplayResourcePopup<Texture>();
    if (result.has_value())
    {
        SafePtr<Texture> newValue = result.value();
        UpdateProperty(property, &newValue);
    }
    ImGui::PopStyleVar();
}

void Inspector::RenderCubeMapProperty(const Property& property, const std::string& id)
{
    SafePtr<CubeMap> cubeMap = *static_cast<SafePtr<CubeMap>*>(property.data);
    std::string name = cubeMap ? cubeMap->GetName() : "None";
    if (ImGui::Button(name.c_str()))
    {
        ImGui::OpenPopup("Resource Popup");
    }
    auto result = DisplayResourcePopup<CubeMap>();
    if (result.has_value())
    {
        SafePtr<CubeMap> newValue = result.value();
        UpdateProperty(property, &newValue);
    }
}

void Inspector::RenderMeshProperty(const Property& property, const std::string& id)
{
    SafePtr<Mesh> mesh = *static_cast<SafePtr<Mesh>*>(property.data);
    std::string name = mesh ? mesh->GetName() : "None";
    if (ImGui::Button(name.c_str()))
    {
        ImGui::OpenPopup("Resource Popup");
    }
    auto result = DisplayResourcePopup<Mesh>();
    if (result.has_value())
    {
        SafePtr<Mesh> newValue = result.value();
        UpdateProperty(property, &newValue);
    }
}

void Inspector::RenderMaterialProperty(const Property& property, const std::string& id)
{
    SafePtr<Material> material = *static_cast<SafePtr<Material>*>(property.data);
    std::string name = material ? material->GetName() : "None";
    if (ImGui::Button(name.c_str()))
    {
        ImGui::OpenPopup("Resource Popup");
    }
    auto result = DisplayResourcePopup<Material>();
    if (result.has_value())
    {
        SafePtr<Material> newValue = result.value();
        UpdateProperty(property, &newValue);
    }
}

void Inspector::RenderShaderProperty(const Property& property, const std::string& id)
{
    SafePtr<Shader> shader = *static_cast<SafePtr<Shader>*>(property.data);
    std::string name = shader ? shader->GetName() : "None";
    if (ImGui::Button(name.c_str()))
    {
        ImGui::OpenPopup("Resource Popup");
    }
    auto result = DisplayResourcePopup<Shader>();
    if (result.has_value())
    {
        SafePtr<Shader> newValue = result.value();
        UpdateProperty(property, &newValue);
    }
}

void Inspector::RenderPostProcessShaderProperty(const Property& property, const std::string& id)
{
    SafePtr<PostProcessShader> ppshader = *static_cast<SafePtr<PostProcessShader>*>(property.data);
    std::string name = ppshader ? ppshader->GetName() : "None";
    if (ImGui::Button(name.c_str()))
    {
        ImGui::OpenPopup("Resource Popup");
    }
    auto result = DisplayResourcePopup<PostProcessShader>();
    if (result.has_value())
    {
        SafePtr<PostProcessShader> newValue = result.value();
        UpdateProperty(property, &newValue);
    }
}

void Inspector::RenderListProperty(const Property& property, const std::string& id)
{
    size_t listSize = GetListSize(property);

    uint32_t removeIndex = -1;
    for (size_t i = 0; i < listSize; ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableSetColumnIndex(0);
        if (ImGui::Button("-"))
        {
            removeIndex = i;
        }
        ImGui::SameLine(0, 5);
        ImGui::Text("[%zu]", i);

        ImGui::TableSetColumnIndex(1);

        Property elementProp = property;
        elementProp.isList = false;
        elementProp.data = GetListElement(property, i);

        ShowProperty(elementProp);

        ImGui::PopID();
        ImGui::TableNextRow();
    }

    if (removeIndex != -1)
    {
        RemoveListElement(property, removeIndex);
    }
}

void* Inspector::GetListElement(const Property& property, size_t index)
{
    if (!property.data)
        return nullptr;

    switch (property.type)
    {
    case PropertyType::Bool:
        {
            auto* list = static_cast<std::vector<bool>*>(property.data);
            // std::vector<bool> is special, need to handle differently
            static bool temp;
            temp = (*list)[index];
            return &temp;
        }
    case PropertyType::Int:
        {
            auto* list = static_cast<std::vector<int>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::Float:
        {
            auto* list = static_cast<std::vector<float>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::Vec2f:
        {
            auto* list = static_cast<std::vector<Vec2f>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::Vec3f:
        {
            auto* list = static_cast<std::vector<Vec3f>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::Vec4f:
        {
            auto* list = static_cast<std::vector<Vec4f>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::Quat:
        {
            auto* list = static_cast<std::vector<Quat>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::Texture:
        {
            auto* list = static_cast<std::vector<SafePtr<Texture>>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::CubeMap:
        {
            auto* list = static_cast<std::vector<SafePtr<CubeMap>>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::Mesh:
        {
            auto* list = static_cast<std::vector<SafePtr<Mesh>>*>(property.data);
            return &(*list)[index];
        }
    case PropertyType::Material:
        {
            auto* list = static_cast<std::vector<SafePtr<Material>>*>(property.data);
            return &(*list)[index];
        }
    default:
        return nullptr;
    }
}

size_t Inspector::GetListSize(const Property& property)
{
    if (!property.data)
        return 0;

    switch (property.type)
    {
    case PropertyType::Bool:
        return static_cast<std::vector<bool>*>(property.data)->size();
    case PropertyType::Int:
        return static_cast<std::vector<int>*>(property.data)->size();
    case PropertyType::Float:
        return static_cast<std::vector<float>*>(property.data)->size();
    case PropertyType::Vec2f:
        return static_cast<std::vector<Vec2f>*>(property.data)->size();
    case PropertyType::Vec3f:
        return static_cast<std::vector<Vec3f>*>(property.data)->size();
    case PropertyType::Vec4f:
        return static_cast<std::vector<Vec4f>*>(property.data)->size();
    case PropertyType::Quat:
        return static_cast<std::vector<Quat>*>(property.data)->size();
    case PropertyType::Texture:
        return static_cast<std::vector<SafePtr<Texture>>*>(property.data)->size();
    case PropertyType::CubeMap:
        return static_cast<std::vector<SafePtr<CubeMap>>*>(property.data)->size();
    case PropertyType::Mesh:
        return static_cast<std::vector<SafePtr<Mesh>>*>(property.data)->size();
    case PropertyType::Material:
        return static_cast<std::vector<SafePtr<Material>>*>(property.data)->size();
    default:
        return 0;
    }
}

void Inspector::RemoveListElement(const Property& property, size_t index)
{
    if (!property.data || property.readOnly)
        return;

    switch (property.type)
    {
    case PropertyType::Bool:
        {
            auto* list = static_cast<std::vector<bool>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Int:
        {
            auto* list = static_cast<std::vector<int>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Float:
        {
            auto* list = static_cast<std::vector<float>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Vec2f:
        {
            auto* list = static_cast<std::vector<Vec2f>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Vec3f:
        {
            auto* list = static_cast<std::vector<Vec3f>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Vec4f:
        {
            auto* list = static_cast<std::vector<Vec4f>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Quat:
        {
            auto* list = static_cast<std::vector<Quat>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Texture:
        {
            auto* list = static_cast<std::vector<SafePtr<Texture>>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::CubeMap:
        {
            auto* list = static_cast<std::vector<SafePtr<CubeMap>>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Mesh:
        {
            auto* list = static_cast<std::vector<SafePtr<Mesh>>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    case PropertyType::Material:
        {
            auto* list = static_cast<std::vector<SafePtr<Material>>*>(property.data);
            if (index < list->size())
                list->erase(list->begin() + index);
            break;
        }
    default:
        PrintError("Type not handled for RemoveListElement");
    }

    if (property.onModified)
        property.onModified();
}

void Inspector::AddListElement(const Property& property)
{
    if (!property.data || property.readOnly)
        return;

    switch (property.type)
    {
    case PropertyType::Bool:
        static_cast<std::vector<bool>*>(property.data)->push_back(false);
        break;
    case PropertyType::Int:
        static_cast<std::vector<int>*>(property.data)->push_back(0);
        break;
    case PropertyType::Float:
        static_cast<std::vector<float>*>(property.data)->push_back(0.0f);
        break;
    case PropertyType::Vec2f:
        static_cast<std::vector<Vec2f>*>(property.data)->emplace_back(0.0f, 0.0f);
        break;
    case PropertyType::Vec3f:
        static_cast<std::vector<Vec3f>*>(property.data)->emplace_back(0.0f, 0.0f, 0.0f);
        break;
    case PropertyType::Vec4f:
        static_cast<std::vector<Vec4f>*>(property.data)->emplace_back(0.0f, 0.0f, 0.0f, 0.0f);
        break;
    case PropertyType::Quat:
        static_cast<std::vector<Quat>*>(property.data)->emplace_back();
        break;
    case PropertyType::Texture:
        static_cast<std::vector<SafePtr<Texture>>*>(property.data)->emplace_back();
        break;
    case PropertyType::CubeMap:
        static_cast<std::vector<SafePtr<CubeMap>>*>(property.data)->emplace_back();
        break;
    case PropertyType::Mesh:
        static_cast<std::vector<SafePtr<Mesh>>*>(property.data)->emplace_back();
        break;
    case PropertyType::Material:
        static_cast<std::vector<SafePtr<Material>>*>(property.data)->emplace_back();
        break;
    default:
        PrintError("Type not handled for AddListElement");
    }

    if (property.onModified)
        property.onModified();
}

void Inspector::DisplayAddComponentPopup() const
{
    auto componentList = Engine::Get()->GetComponentRegister()->GetComponentTypes();
    if (ImGui::BeginPopup("Add Component"))
    {
        Scene* scene = m_sceneHolder->GetCurrentScene();
        SafePtr<GameObject> object = scene->GetGameObject(m_selectedObject);
        ASSERT(object);
        for (auto& [id, info] : componentList)
        {
            if (ImGui::MenuItem(info.GetTypeName()))
            {
                scene->AddComponent(object.getPtr(), id);
                ImGui::CloseCurrentPopup();
                break;
            }
        }
        ImGui::EndPopup();
    }
}

void Inspector::ShowParticleSystem(const Property& property)
{
    auto ps = static_cast<ParticleSystemComponent*>(property.data);

    if (!ps) return;

    ParticleSettings& settings = ps->GetSettings();

    bool changed = false;

    bool isPlaying = ps->IsPlaying();
    if (!isPlaying && ImGui::Button("Play"))
    {
        ps->Play();
    }
    else if (isPlaying && ImGui::Button("Pause"))
    {
        ps->Pause();
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart"))
    {
        ps->Restart();
    }
    float currentTime = ps->GetPlaybackTime();
    if (ImGui::SliderFloat("##PlaybackTime", &currentTime, 0, settings.general.duration))
    {
        ps->SetPlaybackTime(currentTime);
    }

    ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);

    if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ParticleSettings::General& general = settings.general;
        ImGui::InputFloat("Duration", &general.duration);
        ImGui::Checkbox("Looping", &general.looping);
        changed |= ImGui::Checkbox("Prewarm", &general.preWarm);
        changed |= DisplayParticleValue("Start Delay", general.startDelay);
        changed |= DisplayParticleValue("Start Life Time", general.startLifeTime);
        changed |= DisplayParticleValue("Start Speed", general.startSpeed);
        changed |= DisplayParticleValue("Start Size", general.startSize);
        int particleCount = general.particleCount;
        if (ImGui::InputInt("Particle count", &particleCount) && particleCount > 0)
        {
            changed |= true;
            general.particleCount = particleCount;
        }
        changed |= DisplayParticleValue("Start Color", general.startColor);
        changed |= DisplayParticleValue("Gravity Scale", general.gravityScale);
    }

    if (ImGui::CollapsingHeader("Emission"))
    {
        ParticleSettings::Emission& emission = settings.emission;
        changed |= DisplayParticleValue("Rate Over Time", emission.rateOverTime);
    }

    if (ImGui::CollapsingHeader("Shape##Collapsing"))
    {
        ParticleSettings::Shape& shape = settings.shape;
        int index = static_cast<int>(shape.type);
        if (ImGui::Combo("Shape", &index, ParticleSettings::Shape::to_cstr()))
        {
            shape.type = static_cast<ParticleSettings::Shape::Type>(index);
            changed = true;
        }
        switch (shape.type)
        {
        case ParticleSettings::Shape::Type::None:
            break;
        case ParticleSettings::Shape::Type::Sphere:
            changed |= ImGui::DragFloat("Radius", &shape.radius);
            break;
        case ParticleSettings::Shape::Type::Cube:
            break;
        case ParticleSettings::Shape::Type::Cone:
            break;
        }
    }

    if (ImGui::CollapsingHeader("Rendering"))
    {
        ParticleSettings::Rendering& rendering = settings.rendering;
        if (ImGui::Checkbox("Billboard", &rendering.billboard))
        {
            ps->SetBillboard(rendering.billboard);
        }
        {
            auto mat = ps->GetMaterial();
            ClassDescriptor descriptor;
            mat->Describe(descriptor);
            ShowDescriptor(descriptor);
        }
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal);
        {
            ClassDescriptor descriptor;
            Property meshProp;
            meshProp.name = "Mesh";
            meshProp.type = PropertyType::Mesh;
            meshProp.setter = [ps](void* data) { ps->SetMesh(*static_cast<SafePtr<Mesh>*>(data)); };
            SafePtr<Mesh> mesh = ps->GetMesh();
            meshProp.data = &mesh;
            descriptor.AddProperty(meshProp);
            ShowDescriptor(descriptor);
        }
    }

    if (changed)
    {
        ps->ApplySettings();
    }
}
