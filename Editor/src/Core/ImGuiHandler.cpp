#include "ImGuiHandler.h"
#include <iostream>

#include "Core/Window.h"
#include "Core/Window/WindowGLFW.h"
#include "Render/RHI/RHIRenderer.h"

#ifdef RENDER_API_VULKAN
#include "Render/Vulkan/VulkanRenderer.h"
#endif

void ImGuiHandler::Initialize(Window* window, RHIRenderer* renderer)
{
    m_renderer = renderer;
#ifdef RENDER_API_VULKAN
    auto vulkanRenderer = dynamic_cast<VulkanRenderer*>(renderer);
    m_device = vulkanRenderer->GetDevice();
    VkDescriptorPoolSize pool_sizes[] =
    {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    vkCreateDescriptorPool(m_device->GetDevice(), &pool_info, nullptr, &m_descriptorPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard

#ifdef WINDOW_API_GLFW
    ImGui_ImplGlfw_InitForVulkan(dynamic_cast<WindowGLFW*>(window)->GetHandle(), true);
#endif

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vulkanRenderer->GetContext()->GetInstance();
    init_info.PhysicalDevice = m_device->GetPhysicalDevice();
    init_info.Device = m_device->GetDevice();
    init_info.QueueFamily = m_device->GetGraphicsQueueFamily();
    init_info.Queue = m_device->GetGraphicsQueue().handle;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_descriptorPool; // The pool you created above
    init_info.RenderPass = vulkanRenderer->GetRenderPass()->GetRenderPass(); // Your main render pass
    init_info.Subpass = 0;
    init_info.MinImageCount = 2; // >= 2
    init_info.ImageCount = vulkanRenderer->GetSwapChain()->GetImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info);
#endif
}

void ImGuiHandler::Cleanup()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPool != VK_NULL_HANDLE && m_device)
    {
        vkDestroyDescriptorPool(m_device->GetDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
}

void ImGuiHandler::BeginFrame()
{
#ifdef RENDER_API_VULKAN
    ImGui_ImplVulkan_NewFrame();
#endif
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiHandler::EndFrame()
{
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
#ifdef RENDER_API_VULKAN
    VulkanRenderer* vulkanRenderer = dynamic_cast<VulkanRenderer*>(m_renderer);
    VulkanCommandPool* commandPool = vulkanRenderer->GetCommandPool();
    uint32_t m_currentFrame = vulkanRenderer->GetFrameIndex();
    if (draw_data && draw_data->CmdListsCount > 0)
    {
        VkCommandBuffer commandBuffer = commandPool->GetCommandBuffer(m_currentFrame);
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
    }
#endif
}
