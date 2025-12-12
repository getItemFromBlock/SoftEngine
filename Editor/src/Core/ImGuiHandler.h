#pragma once
#ifdef RENDER_API_VULKAN
#include <vulkan/vulkan_core.h>
#endif

#include <imgui.h>
#ifdef WINDOW_API_GLFW
#include <imgui_impl_glfw.h>
#endif
#ifdef RENDER_API_VULKAN
#include <imgui_impl_vulkan.h>
#endif

class Window;
class RHIRenderer;
class VulkanDevice;

class ImGuiHandler
{
public:
    void Initialize(Window* window, RHIRenderer* renderer);
    void Cleanup();
    
    void BeginFrame();
    void EndFrame();

private:
#ifdef RENDER_API_VULKAN
    VkDescriptorPool m_descriptorPool;
    VulkanDevice* m_device;
    RHIRenderer* m_renderer;
#endif
};
