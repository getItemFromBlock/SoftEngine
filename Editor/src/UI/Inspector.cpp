#include "Inspector.h"

#include "Component/MeshComponent.h"
#include "Component/ParticleSystemComponent.h"
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
            size_t i = 0;
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

                bool destroy = true;
                const bool open = ImGui::CollapsingHeader(component->GetTypeName(), &destroy,
                                                          ImGuiTreeNodeFlags_AllowOverlap |
                                                          ImGuiTreeNodeFlags_DefaultOpen);

                if (open)
                {
                    ShowProperty(descriptor);
                }
                ImGui::PopID();
                i++;
            }
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

template<>
void Inspector::DisplayResourcePopup<Material>(const Property& property) const
{
    SafePtr<Material>* materialPtr = static_cast<SafePtr<Material>*>(property.data);
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = p_engine->GetResourceManager();
        auto materials = resourceManager->GetAll<Material>();
        for (auto& material : materials)
        {
            ImGui::PushID(material->GetUUID());
            if (ImGui::MenuItem(material->GetName().c_str()))
            {
                if (property.setter)
                    property.setter(&material);
                else
                    *materialPtr = material;
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
}

template<>
void Inspector::DisplayResourcePopup<Mesh>(const Property& property) const
{
    SafePtr<Mesh>* meshPtr = static_cast<SafePtr<Mesh>*>(property.data);
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = p_engine->GetResourceManager();
        auto meshes = resourceManager->GetAll<Mesh>();
        for (auto& mesh : meshes)
        {
            ImGui::PushID(mesh->GetUUID());
            if (ImGui::MenuItem(mesh->GetName().c_str()))
            {
                if (property.setter)
                    property.setter(&mesh);
                else
                    *meshPtr = mesh;
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
}

template<>
void Inspector::DisplayResourcePopup<Texture>(const Property& property) const
{
    SafePtr<Texture>* texturePtr = static_cast<SafePtr<Texture>*>(property.data);
    if (ImGui::BeginPopup("Resource Popup"))
    {
        auto resourceManager = p_engine->GetResourceManager();
        auto textures = resourceManager->GetAll<Texture>();
        for (auto& texture : textures)
        {
            ImGui::PushID(texture->GetUUID());
            if (ImGui::MenuItem(texture->GetName().c_str()))
            {
                if (property.setter)
                    property.setter(&texture);
                else
                    *texturePtr = texture;
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
}

void Inspector::ShowMaterials(const Property& property)
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
            const ClassDescriptor& descriptor = GetDescriptor(material->GetUUID(), material);
            ShowProperty(descriptor);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    *materials = materialList;
}

void Inspector::ShowMesh(const Property& property)
{
    SafePtr<Mesh>* meshPtr = static_cast<SafePtr<Mesh>*>(property.data);
    SafePtr<Mesh> mesh = *meshPtr;
    auto meshName = mesh->GetName();
    ImGui::TextUnformatted(property.name.c_str());
    ImGui::SameLine();
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
                if (property.setter)
                    property.setter(&mesh);
                else
                    *meshPtr = mesh;
            }
            ImGui::PopID();
        }
        ImGui::EndPopup();
    }
}

void Inspector::ShowTransform(const Property& property)
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

template<typename T>
bool DisplayWithType(const std::string& name, T* value)
{
    ImGui::TextUnformatted("Unknown");
    return false;
}

template<>
bool DisplayWithType(const std::string& name, float* value)
{
    return ImGui::DragFloat(name.c_str(), value);
}

template<>
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
        changed |= ImGui::InputInt("Particle count", &general.particleCount);
        changed |= DisplayParticleValue("Start Color", general.startColor);
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
        switch (shape.type) {
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
        auto mat = ps->GetMaterial();
        const ClassDescriptor& descriptor = GetDescriptor(mat->GetUUID(), mat);
        ShowProperty(descriptor);
    }
    
    if (changed)
    {
        ps->ApplySettings();
    }
}

void Inspector::ShowTexture(const Property& property)
{
    auto texturePtr = static_cast<SafePtr<Texture>*>(property.data);
    if (!texturePtr->getPtr())
        return;
    auto textureID = m_imguiHandler->GetTextureID(texturePtr->getPtr());
    if (ImGui::ImageButton(property.name.c_str(), textureID, ImVec2(64, 64)))
    {
        ImGui::OpenPopup("Resource Popup");
    }
    DisplayResourcePopup<Texture>(property);
}

void Inspector::ShowProperty(const ClassDescriptor& descriptor)
{
    for (auto& property : descriptor.properties)
    {
        switch (property.type)
        {
        case PropertyType::None:
            ImGui::Text("None");
            break;
        case PropertyType::Bool:
        {
            bool local = *static_cast<bool*>(property.data);
            if (ImGui::Checkbox(property.name.c_str(), &local))
            {
                if (property.setter)
                    property.setter(static_cast<void*>(&local));
                else
                    *static_cast<bool*>(property.data) = local;
            }
            break;
        }
        case PropertyType::Int:
        {
            int local = *static_cast<int*>(property.data);
            if (ImGui::InputInt(property.name.c_str(), &local))
            {
                if (property.setter)
                    property.setter(static_cast<void*>(&local));
                else
                    *static_cast<int*>(property.data) = local;
            }
            break;
        }
        case PropertyType::Float:
        {
            float local = *static_cast<float*>(property.data);
            if (ImGui::DragFloat(property.name.c_str(), &local))
            {
                if (property.setter)
                    property.setter(static_cast<void*>(&local));
                else
                    *static_cast<float*>(property.data) = local;
            }
            break;
        }
        case PropertyType::Vec2f:
        {
            Vec2f local = *static_cast<Vec2f*>(property.data);
            if (ImGui::DragFloat2(property.name.c_str(), &local.x))
            {
                if (property.setter)
                    property.setter(static_cast<void*>(&local));
                else
                    *static_cast<Vec2f*>(property.data) = local;
            }
            break;
        }
        case PropertyType::Vec3f:
        {
            Vec3f local = *static_cast<Vec3f*>(property.data);
            if (ImGui::DragFloat3(property.name.c_str(), &local.x))
            {
                if (property.setter)
                    property.setter(static_cast<void*>(&local));
                else
                    *static_cast<Vec3f*>(property.data) = local;
            }
            break;
        }
        case PropertyType::Vec4f:
        {
            Vec4f local = *static_cast<Vec4f*>(property.data);
            if (ImGui::DragFloat4(property.name.c_str(), &local.x))
            {
                if (property.setter)
                    property.setter(static_cast<void*>(&local));
                else
                    *static_cast<Vec4f*>(property.data) = local;
            }
            break;
        }
        case PropertyType::Quat:
        {
            auto quat = static_cast<Quat*>(property.data);
            Quat localQuat = *quat;
            Vec3f euler = localQuat.ToEuler();
            if (ImGui::DragFloat3(property.name.c_str(), &euler.x))
            {
                localQuat = Quat::FromEuler(euler);
                if (property.setter)
                    property.setter(static_cast<void*>(&localQuat));
                else
                    *quat = localQuat;
            }
            break;
        }
        case PropertyType::Color3:
        {
            Vec3f local = *static_cast<Vec3f*>(property.data);
            if (ImGui::ColorEdit3(property.name.c_str(), &local.x))
            {
                if (property.setter)
                    property.setter(static_cast<void*>(&local));
                else
                    *static_cast<Vec3f*>(property.data) = local;
            }
            break;
        }
        case PropertyType::Color4:
        {
            Vec4f local = *static_cast<Vec4f*>(property.data);
            if (ImGui::ColorEdit4(property.name.c_str(), &local.x))
            {
                if (property.setter)
                    property.setter(static_cast<void*>(&local));
                else
                    *static_cast<Vec4f*>(property.data) = local;
            }
            break;
        }
        case PropertyType::Texture:
        {
            ShowTexture(property);
            break;
        }
        case PropertyType::Mesh:
            ShowMesh(property);
            break;
        case PropertyType::Materials:
            ShowMaterials(property);
            break;
        case PropertyType::Transform:
            ShowTransform(property);
            break;
        case PropertyType::ParticleSystem:
            ShowParticleSystem(property);
            break;
        default:
            PrintError("Property type not handle on Inspector");
            break;
        }
    }
}

