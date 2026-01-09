#pragma once
#include "EngineAPI.h"

#include <vector>

#include <vulkan/vulkan_core.h>

class Window;

class ENGINE_API VulkanContext
{
public:
    VulkanContext() = default;
    VulkanContext& operator=(const VulkanContext& other) = default;
    VulkanContext(const VulkanContext&) = default;
    VulkanContext(VulkanContext&&) noexcept = default;
    ~VulkanContext();

    bool Initialize(Window* window);
    void Cleanup();
    std::vector<const char*> GetRequiredExtensions(Window* window) const;
    bool CheckExtensionSupport(const std::vector<const char*>& requiredExtensions) const;

    VkInstance GetInstance() const { return m_instance; }
    VkSurfaceKHR GetSurface() const { return m_surface; }
private:
    bool CreateInstance(Window* window);
    bool CheckValidationLayerSupport() const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    static const char* VkResultToString(VkResult result);

    bool CreateDebugMessenger();
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;
    void DestroyDebugMessenger();

    bool CreateSurface(Window* window);
    void DestroySurface();

private:
    
#ifdef _DEBUG
    const bool m_enableValidationLayers = true;
#else
    const bool m_enableValidationLayers = false;
#endif

    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};