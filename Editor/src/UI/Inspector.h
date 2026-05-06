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

    template <typename T>
    static std::optional<SafePtr<T>> DisplayResourcePopup();
private:
    static void ShowProperty(const Property& property);
    static void UpdateProperty(const Property& property, void* newValue);
    
    #pragma region Property Renderers
    static void RenderButtonProperty(const Property& property, const std::string& id);
    static void RenderBoolProperty(const Property& property, const std::string& id);
    static void RenderIntProperty(const Property& property, const std::string& id);
    static void RenderIVec2Property(const Property& property, const std::string& id);
    static void RenderIVec3Property(const Property& property, const std::string& id);
    static void RenderIVec4Property(const Property& property, const std::string& id);
    static void RenderFloatProperty(const Property& property, const std::string& id);
    static void RenderFVec2Property(const Property& property, const std::string& id);
    static void RenderFVec3Property(const Property& property, const std::string& id);
    static void RenderFVec4Property(const Property& property, const std::string& id);
    static void RenderQuatProperty(const Property& property, const std::string& id);
    static void RenderColor3Property(const Property& property, const std::string& id);
    static void RenderColor4Property(const Property& property, const std::string& id);
    static void RenderEnumProperty(const Property& property, const std::string& id);
    static void RenderTextureProperty(const Property& property, const std::string& id);
    static void RenderCubeMapProperty(const Property& property, const std::string& id);
    static void RenderMeshProperty(const Property& property, const std::string& id);
    static void RenderMaterialProperty(const Property& property, const std::string& id);
    static void RenderShaderProperty(const Property& property, const std::string& id);
    static void RenderPostProcessShaderProperty(const Property& property, const std::string& id);
    static void RenderListProperty(const Property& property, const std::string& id);
    #pragma endregion 
    
    static void* GetListElement(const Property& property, size_t index);
    static size_t GetListSize(const Property& property);
    static void RemoveListElement(const Property& property, size_t index);
    static void AddListElement(const Property& property);

    template<typename T>
    static ClassDescriptor GetDescriptor(SafePtr<T> descriptorContainer)
    {
        ClassDescriptor descriptor;
        descriptorContainer->Describe(descriptor);
        return descriptor;
    }
    
    void DisplayAddComponentPopup() const;
    
    static void ShowParticleSystem(const Property& property);
    
private:
    SceneHolder* m_sceneHolder;
    
    Core::UUID m_selectedObject;
    
    std::unordered_map<Core::UUID, ClassDescriptor> m_descriptors;
};
