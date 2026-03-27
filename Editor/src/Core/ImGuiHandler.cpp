#include "ImGuiHandler.h"
#include <iostream>
#include <ranges>

#include "Core/Window.h"
#include "Core/Window/WindowGLFW.h"

#include "Render/Vulkan/VulkanTexture.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Resource/CubeMap.h"

// Validation callback (like in the example)
static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

void SetupStyle()
{
    ImGuiStyle* style = &ImGui::GetStyle();
    ImVec4* colors = style->Colors;

    // Base colors for a pleasant and modern dark theme with dark accents
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.93f, 0.94f, 1.00f); // Light grey text for readability
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.52f, 0.54f, 1.00f); // Subtle grey for disabled text
    colors[ImGuiCol_WindowBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Dark background with a hint of blue
    colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Slightly lighter for child elements
    colors[ImGuiCol_PopupBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f); // Popup background
    colors[ImGuiCol_Border] = ImVec4(0.28f, 0.29f, 0.30f, 0.60f); // Soft border color
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f); // No border shadow
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Frame background
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.26f, 1.00f); // Frame hover effect
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // Active frame background
    colors[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Title background
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Active title background
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f); // Collapsed title background
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f); // Menu bar background
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.18f, 1.00f); // Scrollbar background
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.26f, 0.28f, 1.00f); // Dark accent for scrollbar grab
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.30f, 0.32f, 1.00f); // Scrollbar grab hover
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.32f, 0.34f, 0.36f, 1.00f); // Scrollbar grab active
    colors[ImGuiCol_CheckMark] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Dark blue checkmark
    colors[ImGuiCol_SliderGrab] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Dark blue slider grab
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Active slider grab
    colors[ImGuiCol_Button] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Dark blue button
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Button hover effect
    colors[ImGuiCol_ButtonActive] = ImVec4(0.32f, 0.42f, 0.52f, 1.00f); // Active button
    colors[ImGuiCol_Header] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Header color similar to button
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Header hover effect
    colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.42f, 0.52f, 1.00f); // Active header
    colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.29f, 0.30f, 1.00f); // Separator color
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Hover effect for separator
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Active separator
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Resize grip
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Hover effect for resize grip
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.44f, 0.54f, 0.64f, 1.00f); // Active resize grip
    colors[ImGuiCol_Tab] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Inactive tab
    colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.38f, 0.48f, 1.00f); // Hover effect for tab
    colors[ImGuiCol_TabActive] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Active tab color
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Unfocused tab
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.24f, 0.34f, 0.44f, 1.00f); // Active but unfocused tab
    colors[ImGuiCol_PlotLines] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Plot lines
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Hover effect for plot lines
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.36f, 0.46f, 0.56f, 1.00f); // Histogram color
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.40f, 0.50f, 0.60f, 1.00f); // Hover effect for histogram
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Table header background
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.28f, 0.29f, 0.30f, 1.00f); // Strong border for tables
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.24f, 0.25f, 0.26f, 1.00f); // Light border for tables
    colors[ImGuiCol_TableRowBg] = ImVec4(0.20f, 0.22f, 0.24f, 1.00f); // Table row background
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.22f, 0.24f, 0.26f, 1.00f); // Alternate row background
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.24f, 0.34f, 0.44f, 0.35f); // Selected text background
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.46f, 0.56f, 0.66f, 0.90f); // Drag and drop target
    colors[ImGuiCol_NavHighlight] = ImVec4(0.46f, 0.56f, 0.66f, 1.00f); // Navigation highlight
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f); // Windowing highlight
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f); // Dim background for windowing
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f); // Dim background for modal windows

    // Style adjustments
    style->WindowPadding = ImVec2(8.00f, 8.00f);
    style->FramePadding = ImVec2(5.00f, 2.00f);
    style->CellPadding = ImVec2(6.00f, 6.00f);
    style->ItemSpacing = ImVec2(6.00f, 6.00f);
    style->ItemInnerSpacing = ImVec2(6.00f, 6.00f);
    style->TouchExtraPadding = ImVec2(0.00f, 0.00f);
    style->IndentSpacing = 25;
    style->ScrollbarSize = 11;
    style->GrabMinSize = 10;
    style->WindowBorderSize = 1;
    style->ChildBorderSize = 1;
    style->PopupBorderSize = 1;
    style->FrameBorderSize = 1;
    style->TabBorderSize = 1;
    style->WindowRounding = 7;
    style->ChildRounding = 4;
    style->FrameRounding = 3;
    style->PopupRounding = 4;
    style->ScrollbarRounding = 9;
    style->GrabRounding = 3;
    style->LogSliderDeadzone = 4;
    style->TabRounding = 4;
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

    // SetupStyle();
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

void ImGuiHandler::UpdateTextureID(const Texture* texture)
{
    auto it = m_textureIDs.find(texture->GetUUID());
    if (it != m_textureIDs.end())
    {
        ImGui_ImplVulkan_RemoveTexture(it->second);
        m_textureIDs.erase(it);
    }

    auto buffer = texture->GetBuffer();
    VkDescriptorSet ID = ImGui_ImplVulkan_AddTexture(
        buffer->GetSampler(),
        buffer->GetImageView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );
    m_textureIDs[texture->GetUUID()] = ID;
}

void ImGuiHandler::RemoveTextureID(const Texture* texture)
{
    auto it = m_textureIDs.find(texture->GetUUID());
    if (it != m_textureIDs.end())
    {
        ImGui_ImplVulkan_RemoveTexture(it->second);
        m_textureIDs.erase(it);
    }
}

ImTextureRef ImGuiHandler::GetTextureID(Texture* texture)
{
    if (!texture)
        return {};
    VkDescriptorSet ID = {};
    auto it = m_textureIDs.find(texture->GetUUID());
    if (it == m_textureIDs.end())
    {
        m_eventHandles[texture->GetUUID()] = texture->EOnSentToGPU.Bind([this, texture]()
        {
            RemoveTextureID(texture);
        });

        auto buffer = texture->GetBuffer();
        if (!buffer)
            return {};

        VkDescriptorSet newID = ImGui_ImplVulkan_AddTexture(
            buffer->GetSampler(),
            buffer->GetImageView(),
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        m_textureIDs[texture->GetUUID()] = newID;

        return {newID};
    }
    else
    {
        ID = it->second;
    }
    return {reinterpret_cast<ImTextureID>(ID)};
}
