#include "ImGuiHandler.h"
#include <iostream>
#include <ranges>

#include "Core/Window.h"
#include "Core/Window/WindowGLFW.h"

#include "Render/Vulkan/VulkanTexture.h"
#include "Render/Vulkan/VulkanRenderer.h"

// Validation callback (like in the example)
static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void ImGuiHandler::Initialize(Window* window, VulkanRenderer* renderer)
{
    m_renderer = renderer;
    m_device = renderer->GetDevice();
    
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
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkResult err = vkCreateDescriptorPool(m_device->GetDevice(), &pool_info, nullptr, &m_descriptorPool);
    check_vk_result(err);

    GLFWwindow* glfwWindow = dynamic_cast<WindowGLFW*>(window)->GetHandle();
    
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForVulkan(glfwWindow, true);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = renderer->GetContext()->GetInstance();
    init_info.PhysicalDevice = m_device->GetPhysicalDevice();
    init_info.Device = m_device->GetDevice();
    init_info.QueueFamily = m_device->GetGraphicsQueueFamily();
    init_info.Queue = m_device->GetGraphicsQueue().handle;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = m_descriptorPool;
    init_info.MinImageCount = 2;
    init_info.ImageCount = renderer->GetSwapChain()->GetImageCount();
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = check_vk_result;
    
    init_info.UseDynamicRendering = true;
    
    VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info = {};
    pipeline_rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipeline_rendering_create_info.colorAttachmentCount = 1;
    VkFormat colorFormat = renderer->GetRenderPass()->GetColorFormat();
    pipeline_rendering_create_info.pColorAttachmentFormats = &colorFormat;
    pipeline_rendering_create_info.depthAttachmentFormat = renderer->GetRenderPass()->GetDepthFormat();
    
    init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering_create_info;

    bool result = ImGui_ImplVulkan_Init(&init_info);
    if (!result)
    {
        std::cerr << "Failed to initialize ImGui Vulkan backend!" << std::endl;
    }
    
    VkPhysicalDeviceProperties deviceProps;
    vkGetPhysicalDeviceProperties(m_device->GetPhysicalDevice(), &deviceProps);
}

void ImGuiHandler::Cleanup()
{
    for (auto& textureID : m_textureIDs | std::views::values)
    {
        ImGui_ImplVulkan_RemoveTexture(textureID);
    }
    m_textureIDs.clear();
    
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
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiHandler::EndFrame()
{
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();
    
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized && draw_data->CmdListsCount > 0)
    {
        VulkanCommandPool* commandPool = m_renderer->GetCommandPool();
        uint32_t currentFrame = m_renderer->GetFrameIndex();
        VkCommandBuffer commandBuffer = commandPool->GetCommandBuffer(currentFrame);
        
        ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
    }
}

ImTextureRef ImGuiHandler::GetTextureID(Texture* texture)
{
    VkDescriptorSet ID;
    auto it = m_textureIDs.find(texture->GetUUID());
    if (it == m_textureIDs.end())
    {
        auto buffer = texture->GetBuffer();
        ID = ImGui_ImplVulkan_AddTexture(
            buffer->GetSampler(),
            buffer->GetImageView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        m_textureIDs[texture->GetUUID()] = ID;
    }
    else
    {
        ID = it->second;
    }
    return ImTextureRef(reinterpret_cast<ImTextureID>(ID));
}