#pragma once
#include "EditorWindow.h"
#include "Core/UUID.h"
#include "Scene/ClassDescriptor.h"
#include "Utils/Type.h"

class SceneHolder;

class Inspector : public EditorWindow
{
public:
    Inspector(Engine* engine, ImGuiHandler* handler);
    
    void OnRender() override;
    
    void SetSelectedObject(const Core::UUID& uuid);

    void ShowProperty(const ClassDescriptor& descriptor);
private:
    template <typename T>
    void DisplayResourcePopup(const Property& property) const;
    
    void ShowMaterials(const Property& property);
    void ShowMesh(const Property& property);
    void ShowTransform(const Property& property);
    void ShowParticleSystem(const Property& property);
    void ShowTexture(const Property& property);

    template<typename T>
    const ClassDescriptor& GetDescriptor(const Core::UUID& uuid, SafePtr<T> descriptorContainer)
    {
        // auto it = m_descriptors.find(uuid);
        // if (it == m_descriptors.end())
        // {
            ClassDescriptor descriptor;
            descriptorContainer->Describe(descriptor);
            return m_descriptors[uuid] = descriptor;
        // }
        // return it->second;
    }
private:
    SceneHolder* m_sceneHolder;
    
    Core::UUID m_selectedObject;
    
    std::unordered_map<Core::UUID, ClassDescriptor> m_descriptors;
};
