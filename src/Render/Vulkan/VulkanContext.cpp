#include "VulkanContext.h"
#include "Core/Window.h"

#include <stdexcept>
#include <vector>
#include <cstring>
#include <iostream>
#include <string>
#include <algorithm>

#include "Debug/Log.h"

VulkanContext::~VulkanContext()
{
    Cleanup();
}

bool VulkanContext::Initialize(Window* window)
{
    if (!window)
    {
        PrintError("Invalid window pointer!");
        return false;
    }

    try
    {
        if (!CreateInstance(window))
        {
            Cleanup();
            return false;
        }

        if (m_enableValidationLayers && !CreateDebugMessenger())
        {
            Cleanup();
            return false;
        }

        if (!CreateSurface(window))
        {
            Cleanup();
            return false;
        }

        PrintLog("Vulkan context initialized successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        PrintError("VulkanContext initialization failed: %s", e.what());
        Cleanup();
        return false;
    }
}

void VulkanContext::Cleanup()
{
    DestroySurface();
    DestroyDebugMessenger();

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

bool VulkanContext::CreateInstance(Window* window)
{
    if (m_enableValidationLayers && !CheckValidationLayerSupport())
    {
        PrintError("Validation layers requested, but not available!");
        return false;
    }

    std::string appName = window->GetTitle();

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = appName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Custom Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> extensions = GetRequiredExtensions(window);

    if (!CheckExtensionSupport(extensions))
    {
        PrintError("Required Vulkan extensions not available!");
        return false;
    }

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_enableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        createInfo.ppEnabledLayerNames = m_validationLayers.data();

        PopulateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }
    else
    {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }

#if defined(__APPLE__)
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        PrintError("Failed to create Vulkan instance! Error code: %s", VkResultToString(result));
        return false;
    }

    return true;
}

bool VulkanContext::CreateSurface(Window* window)
{
    if (m_instance == VK_NULL_HANDLE)
    {
        PrintError("Cannot create surface: instance is null!");
        return false;
    }

    m_surface = window->CreateSurface(m_instance);

    if (m_surface == VK_NULL_HANDLE)
    {
        PrintError("Failed to create Vulkan surface!");
        return false;
    }

    return true;
}

bool VulkanContext::CreateDebugMessenger()
{
    if (!m_enableValidationLayers || m_instance == VK_NULL_HANDLE)
    {
        return true;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    PopulateDebugMessengerCreateInfo(createInfo);

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

    if (!func)
    {
        PrintError("Failed to load vkCreateDebugUtilsMessengerEXT function!");
        return false;
    }

    VkResult result = func(m_instance, &createInfo, nullptr, &m_debugMessenger);
    if (result != VK_SUCCESS)
    {
        PrintError("Failed to set up debug messenger! Error code: %s", VkResultToString(result));
        return false;
    }

    return true;
}

void VulkanContext::DestroySurface()
{
    if (m_surface == VK_NULL_HANDLE || m_instance == VK_NULL_HANDLE)
        return;

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
}

void VulkanContext::DestroyDebugMessenger()
{
    if (m_debugMessenger == VK_NULL_HANDLE || m_instance == VK_NULL_HANDLE)
        return;

    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

    if (func != nullptr)
    {
        func(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }
}

std::vector<const char*> VulkanContext::GetRequiredExtensions(Window* window) const
{
    std::vector<const char*> extensions = window->GetRequiredExtensions();
#if defined(__APPLE__)
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    if (m_enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool VulkanContext::CheckExtensionSupport(const std::vector<const char*>& requiredExtensions) const
{
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    if (extensionCount == 0)
    {
        PrintError("No Vulkan extensions available!");
        return false;
    }

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    for (const char* required : requiredExtensions)
    {
        bool found = std::ranges::any_of(availableExtensions,
                                         [required](const VkExtensionProperties& ext)
                                         {
                                             return strcmp(required, ext.extensionName) == 0;
                                         });

        if (!found)
        {
            PrintError((std::string("Required extension not available: ") + required).c_str());
            return false;
        }
    }

    return true;
}

bool VulkanContext::CheckValidationLayerSupport() const
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    if (layerCount == 0)
    {
        return false;
    }

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : m_validationLayers)
    {
        bool layerFound = std::any_of(availableLayers.begin(), availableLayers.end(),
                                      [layerName](const VkLayerProperties& layer)
                                      {
                                          return strcmp(layerName, layer.layerName) == 0;
                                      });

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

void VulkanContext::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
    createInfo.pUserData = nullptr;
}

VkBool32 VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    UNUSED(messageType);
    UNUSED(pUserData);

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        PrintError("Validation layer: %s", pCallbackData->pMessage);
    }
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        PrintWarning("Validation layer: %s", pCallbackData->pMessage);
    }
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        PrintLog("Validation layer: %s", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

const char* VulkanContext::VkResultToString(VkResult result)
{
    switch (result)
    {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
    default: return ("UNKNOWN_ERROR (" + std::to_string(result) + ")").c_str();
    }
}
