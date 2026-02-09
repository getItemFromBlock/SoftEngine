#pragma once
#include <vulkan/vulkan_core.h>

#include <imgui.h>
#include <unordered_map>

#include "Core/UUID.h"
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

class CubeMap;
class Texture;
class Window;
class VulkanRenderer;
class VulkanDevice;

class ImGuiHandler
{
public:
    void Initialize(Window* window, VulkanRenderer* renderer);
    void Cleanup();
    
    void BeginFrame();
    void EndFrame();

    ImTextureRef GetTextureID(Texture* texture);
    ImTextureRef GetCubeMapID(CubeMap* cubeMap);
private:
    VkDescriptorPool m_descriptorPool;
    VulkanDevice* m_device;
    VulkanRenderer* m_renderer;
    
    std::unordered_map<Core::UUID, VkDescriptorSet> m_textureIDs;
};
