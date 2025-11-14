#include "VulkanContext.h"

#include "Core/Window.h"

#include <stdexcept>
#include <vector>
#include <cstring>
#include <iostream>
#include <string>

bool VulkanContext::Initialize(Window* window)
{
    if (!window)
    {
        std::cerr << "Invalid window pointer!" << std::endl;
        return false;
    }

    try
    {
        CreateInstance(window);

        if (m_enableValidationLayers)
        {
            CreateDebugMessenger();
        }

        CreateSurface(window);

        if (m_surface == VK_NULL_HANDLE)
        {
            std::cerr << "Failed to create Vulkan surface!" << std::endl;
            Cleanup();
            return false;
        }

        std::cout << "Vulkan context initialized successfully" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "VulkanContext initialization failed: " << e.what() << std::endl;
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

std::vector<const char*> VulkanContext::GetRequiredExtensions(Window* window) const
{
    std::vector<const char*> extensions = window->GetRequiredExtensions();
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

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

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    for (const char* required : requiredExtensions)
    {
        bool found = false;
        for (const auto& available : availableExtensions)
        {
            if (strcmp(required, available.extensionName) == 0)
            {
                found = true;
                break;
            }
        }
        
        if (!found)
        {
            std::cerr << "Required extension not available: " << required << std::endl;
            return false;
        }
    }

    return true;
}

void VulkanContext::CreateInstance(Window* window)
{
    if (m_enableValidationLayers && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("Validation layers requested, but not available!");
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
        throw std::runtime_error("Required Vulkan extensions not available!");
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

    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance! Error code: " + 
                                 std::to_string(result));
    }
}

bool VulkanContext::CheckValidationLayerSupport() const
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : m_validationLayers)
    {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }
        if (!layerFound)
        {
            return false;
        }
    }
    return true;
}

VkBool32 VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    const char* severityStr = "";
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        severityStr = "[ERROR] ";
    }
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        severityStr = "[WARNING] ";
    }
    else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        severityStr = "[INFO] ";
    }

    std::cerr << severityStr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    
    return VK_FALSE;
}

void VulkanContext::CreateDebugMessenger()
{
    if (!m_enableValidationLayers || m_instance == VK_NULL_HANDLE)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    PopulateDebugMessengerCreateInfo(createInfo);

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    
    if (!func)
    {
        throw std::runtime_error("Failed to load vkCreateDebugUtilsMessengerEXT function!");
    }

    VkResult result = func(m_instance, &createInfo, nullptr, &m_debugMessenger);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to set up debug messenger! Error code: " + 
                                 std::to_string(result));
    }
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

void VulkanContext::CreateSurface(Window* window)
{
    if (m_instance == VK_NULL_HANDLE)
        return;

    m_surface = window->CreateSurface(m_instance);

    if (m_surface == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to create Vulkan surface!");
    }
}

void VulkanContext::DestroySurface()
{
    if (m_surface == VK_NULL_HANDLE)
        return;

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;
}
