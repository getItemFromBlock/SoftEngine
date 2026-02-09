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

    static void ShowDescriptor(const ClassDescriptor& descriptor);
private:
    static void ShowProperty(const Property& property);
    static void UpdateProperty(const Property& property, void* newValue);
    
    static void RenderBoolProperty(const Property& property, const std::string& id);
    static void RenderIntProperty(const Property& property, const std::string& id);
    static void RenderFloatProperty(const Property& property, const std::string& id);
    static void RenderVec2Property(const Property& property, const std::string& id);
    static void RenderVec3Property(const Property& property, const std::string& id);
    static void RenderVec4Property(const Property& property, const std::string& id);
    static void RenderQuatProperty(const Property& property, const std::string& id);
    static void RenderColor3Property(const Property& property, const std::string& id);
    static void RenderColor4Property(const Property& property, const std::string& id);
    static void RenderTextureProperty(const Property& property, const std::string& id);
    static void RenderCubeMapProperty(const Property& property, const std::string& id);
    static void RenderMeshProperty(const Property& property, const std::string& id);
    static void RenderMaterialProperty(const Property& property, const std::string& id);
    static void RenderListProperty(const Property& property, const std::string& id);
    
    static void* GetListElement(const Property& property, size_t index);
    static size_t GetListSize(const Property& property);
    static void RemoveListElement(const Property& property, size_t index);
    static void AddListElement(const Property& property);

    template <typename T>
    static SafePtr<T> DisplayResourcePopup();
    
    void ShowMaterials(const Property& property);
    void ShowMesh(const Property& property);
    void ShowTransform(const Property& property);
    void ShowParticleSystem(const Property& property);
    void ShowTexture(const Property& property);
    void ShowCubeMap(const Property& property);

    template<typename T>
    const ClassDescriptor& GetDescriptor(const Core::UUID& uuid, SafePtr<T> descriptorContainer)
    {
        ClassDescriptor descriptor;
        descriptorContainer->Describe(descriptor);
        return m_descriptors[uuid] = descriptor;
    }
    
    
private:
    SceneHolder* m_sceneHolder;
    
    Core::UUID m_selectedObject;
    
    std::unordered_map<Core::UUID, ClassDescriptor> m_descriptors;
};
