#include "VulkanRenderer.h"
#ifdef RENDER_API_VULKAN

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanFramebuffer.h"
#include "VulkanCommandPool.h"
#include "VulkanSyncObjects.h"
#include "VulkanDescriptorPool.h"
#include "VulkanDescriptorSet.h"

#include "Core/Window.h"

#include <iostream>
#include <stdexcept>
#include <chrono>
#include <ranges>
#include <spirv_reflect.h>
#include <shaderc/shaderc.hpp>

// using namespace GALAXY::Math;
// #include <galaxymath/Maths.h>
//
// #include <imgui.h>
// #include <imgui_impl_glfw.h>
// #include <imgui_impl_vulkan.h>

#include "VulkanDepthBuffer.h"
#include "VulkanDescriptorSetLayout.h"
#include "VulkanIndexBuffer.h"
#include "VulkanShaderBuffer.h"
#include "VulkanTexture.h"
#include "VulkanVertexBuffer.h"

#include "Debug/Log.h"
#include "Resource/FragmentShader.h"

#include "Resource/Mesh.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"
#include "Resource/Shader.h"
#include "Resource/VertexShader.h"

#include "Utils/Type.h"


#include "Core/Window/WindowGLFW.h"

VulkanRenderer::~VulkanRenderer() = default;

bool VulkanRenderer::Initialize(Window* window)
{
    if (!window)
    {
        PrintError("Invalid window pointer");
        return false;
    }

    m_window = window;

    try
    {
        m_context = std::make_unique<VulkanContext>();
        if (!m_context->Initialize(window))
        {
            PrintError("Failed to initialize Vulkan context!");
            return false;
        }

        m_device = std::make_unique<VulkanDevice>();
        if (!m_device->Initialize(m_context->GetInstance(), m_context->GetSurface()))
        {
            PrintError("Failed to initialize Vulkan device!");
            return false;
        }

        m_swapChain = std::make_unique<VulkanSwapChain>();
        if (!m_swapChain->Initialize(m_device.get(), m_context->GetSurface(), window))
        {
            PrintError("Failed to initialize swap chain!");
            return false;
        }

        m_renderPass = std::make_unique<VulkanRenderPass>();
        if (!m_renderPass->Initialize(m_device.get(), m_swapChain->GetImageFormat()))
        {
            PrintError("Failed to initialize render pass!");
            return false;
        }

        m_depthBuffer = std::make_unique<VulkanDepthBuffer>();
        if (!m_depthBuffer->Initialize(m_device.get(), m_swapChain->GetExtent()))
        {
            PrintError("Failed to initialize depth buffer!");
            return false;
        }

        m_framebuffer = std::make_unique<VulkanFramebuffer>();
        if (!m_framebuffer->Initialize(m_device.get(), m_renderPass->GetRenderPass(),
                                       m_swapChain->GetImageViews(),
                                       m_swapChain->GetExtent(), m_depthBuffer.get()))
        {
            PrintError("Failed to initialize framebuffers!");
            return false;
        }
        
        m_commandPool = std::make_unique<VulkanCommandPool>();
        if (!m_commandPool->Initialize(m_device.get(), MAX_FRAMES_IN_FLIGHT))
        {
            PrintError("Failed to initialize command buffers!");
            return false;
        }

        m_syncObjects = std::make_unique<VulkanSyncObjects>();
        if (!m_syncObjects->Initialize(m_device.get(), MAX_FRAMES_IN_FLIGHT))
        {
            PrintError("Failed to initialize sync objects!");
            return false;
        }
        m_syncObjects->ResizeRenderFinishedSemaphores(m_swapChain->GetImageCount());

        p_initialized = true;

        window->EResizeEvent.Bind([this](Vec2i)
        {
            m_framebufferResized = true;
        });
        
        /*
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        vkCreateDescriptorPool(m_device->GetDevice(), &pool_info, nullptr, &m_imGuiPool);
        
        // 1. Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard

        // 2. Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(dynamic_cast<WindowGLFW*>(window)->GetHandle(), true); // Replace with ImplSDL2 if using SDL

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_context->GetInstance();
        init_info.PhysicalDevice = m_device->GetPhysicalDevice();
        init_info.Device = m_device->GetDevice();
        init_info.QueueFamily = m_device->GetGraphicsQueueFamily();
        init_info.Queue = m_device->GetGraphicsQueue().handle;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = m_imGuiPool; // The pool you created above
        init_info.RenderPass = m_renderPass->GetRenderPass();    // Your main render pass
        init_info.Subpass = 0;
        init_info.MinImageCount = 2;          // >= 2
        init_info.ImageCount = m_swapChain->GetImageCount();
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info);
        */

        PrintLog("Vulkan renderer initialized successfully");
        return true;
    }
    catch (const std::exception& e)
    {
        PrintError("Vulkan renderer initialization failed: %s", e.what());
        Cleanup();
        return false;
    }
}

void VulkanRenderer::WaitForGPU()
{
    if (m_device)
    {
        vkDeviceWaitIdle(m_device->GetDevice());
    }
}

void VulkanRenderer::Cleanup()
{
    // ImGui_ImplVulkan_Shutdown();
    // ImGui_ImplGlfw_Shutdown();
    // ImGui::DestroyContext();

    if (m_imGuiPool != VK_NULL_HANDLE && m_device)
    {
        vkDestroyDescriptorPool(m_device->GetDevice(), m_imGuiPool, nullptr);
        m_imGuiPool = VK_NULL_HANDLE;
    }
    
    m_syncObjects.reset();
    m_commandPool.reset();
    m_framebuffer.reset();
    m_depthBuffer.reset();
    m_renderPass.reset();
    m_swapChain.reset();
    m_device.reset();
    m_context.reset();

    p_initialized = false;
    PrintLog("Vulkan renderer cleaned up");
}

void VulkanRenderer::WaitUntilFrameFinished()
{
    m_syncObjects->WaitForFence(m_currentFrame);
}

void VulkanRenderer::Update()
{
    /*static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;

    static float time = 0.0f;
    time += deltaTime;

    // float fps = 1.0f / deltaTime;
    // PrintLog("FPS:   %f", fps);

    UniformBufferObject ubo;
    Vec3f camPos = Vec3f(2.0f, 2.0f, 2.0f);
    Vec3f camTarget = Vec3f(0.0f, 0.0f, 0.0f);
    Vec3f camUp = Vec3f(0.0f, 1.0f, 0.0f);

    float distanceInFront = 5.f;
    Vec3f forward = Vec3f::Normalize(camTarget - camPos);
    Vec3f cubePosition = camPos + forward * distanceInFront;

    float angle = time * 90.0f;
    ubo.Model = Mat4::CreateTransformMatrix(cubePosition, Vec3f(0.f, angle, 0.f), Vec3f(1.f, 1.f, 1.f));

    Quat camRotation = Quat::LookRotation(camTarget - camPos, camUp);

    Mat4 out = Mat4::CreateTransformMatrix(camPos, camRotation, Vec3f(1, 1, 1));
    ubo.View = Mat4::LookAtRH(camPos, camTarget, camUp);

    ubo.Projection = Mat4::CreateProjectionMatrix(
        45.f, m_swapChain->GetExtent().width / (float)m_swapChain->GetExtent().height, 0.1f, 10.0f);
    ubo.Projection[1][1] *= -1; // GLM -> Vulkan Y flip

    m_uniformBuffer->WriteToMapped(&ubo, sizeof(ubo), m_currentFrame);*/
}

bool VulkanRenderer::BeginFrame()
{
    m_imageIndex = 0;
    VkResult result = m_swapChain->AcquireNextImage(
        m_syncObjects->GetImageAvailableSemaphore(m_currentFrame),
        &m_imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    m_syncObjects->ResetFence(m_currentFrame);

    m_commandPool->Reset(m_currentFrame);
    m_commandPool->BeginRecording(m_currentFrame);
    
    std::mutex& mutex = m_commandPool->GetMutex();
    mutex.lock();
    
    // Start a new ImGui frame
    // ImGui_ImplVulkan_NewFrame();
    // ImGui_ImplGlfw_NewFrame();
    // ImGui::NewFrame();
    return true;
}

void VulkanRenderer::DrawFrame()
{
}

bool VulkanRenderer::MultiThreadSendToGPU()
{
#ifdef MULTI_THREAD
    return true;
#else
    return false;
#endif
}

void VulkanRenderer::EndFrame()
{    
    // ImGui::Render();
    // ImDrawData* draw_data = ImGui::GetDrawData();
    // if (draw_data && draw_data->CmdListsCount > 0)
    // {
        // VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
        // ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
    // }
    
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    m_renderPass->End(commandBuffer);
    
    auto& mutex = m_commandPool->GetMutex();
    mutex.unlock();
    m_commandPool->EndRecording(m_currentFrame);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_syncObjects->GetImageAvailableSemaphore(m_currentFrame)};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    VkCommandBuffer cmdBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkSemaphore signalSemaphores[] = {m_syncObjects->GetRenderFinishedSemaphore(m_imageIndex)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    VkResult result;
    {        
        std::scoped_lock lock(*m_device->GetGraphicsQueue().mutex);
        result = vkQueueSubmit(m_device->GetGraphicsQueue().handle, 1, &submitInfo,
                               m_syncObjects->GetInFlightFence(m_currentFrame));
    }
    
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present
    result = m_swapChain->PresentImage(m_device->GetPresentQueue(), m_imageIndex,
                                       m_syncObjects->GetRenderFinishedSemaphore(m_imageIndex));

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized)
    {
        m_framebufferResized = false;
        RecreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::SendPushConstants(void* data, size_t size, Shader* shader, PushConstant pushConstant) const
{
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(),
        pushConstant.shaderType == ShaderType::Vertex ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT, 
        pushConstant.offset, size, data);
}

void VulkanRenderer::BindVertexBuffers(RHIVertexBuffer* _vertexBuffer, RHIIndexBuffer* _indexBuffer) const
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    VulkanVertexBuffer* vertexBuffer = static_cast<VulkanVertexBuffer*>(_vertexBuffer);
    VulkanIndexBuffer* indexBuffer = static_cast<VulkanIndexBuffer*>(_indexBuffer);

    VkBuffer vkVertexBuffer = vertexBuffer->GetBuffer();
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vkVertexBuffer, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, indexBuffer->GetIndexType());
}

void VulkanRenderer::DrawVertex(RHIVertexBuffer* _vertexBuffer, RHIIndexBuffer* _indexBuffer)
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    VulkanIndexBuffer* indexBuffer = static_cast<VulkanIndexBuffer*>(_indexBuffer);
    
    vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), 1, 0, 0, 0);
}

std::string VulkanRenderer::CompileShader(ShaderType type, const std::string& code)
{
    shaderc_shader_kind kind;
    switch (type)
    {
    case ShaderType::Vertex:
        kind = shaderc_vertex_shader;
        break;
    case ShaderType::Fragment:
        kind = shaderc_fragment_shader;
        break;
    default:
        PrintError("Invalid shader type");
        return "";    
    }
    
    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(code, kind, "shader.glsl", options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
        PrintError("Shader compilation failed: %s", module.GetErrorMessage().c_str());;
        return {};
    }

    std::vector<uint32_t> spirv(module.begin(), module.end());

    const char* begin = reinterpret_cast<const char*>(spirv.data());
    const char* end = begin + spirv.size() * sizeof(uint32_t);
    return std::string(begin, end);
}

static void ParseBlockVariable(const SpvReflectBlockVariable* var, UniformMember &out)
{
    if (!var) return;

    if (var->name && var->name[0]) out.name = var->name;
    else if (var->type_description && var->type_description->type_name)
        out.name = var->type_description->type_name;
    else
        out.name = "";

    if (var->type_description && var->type_description->type_name)
        out.typeName = var->type_description->type_name;
    else
        out.typeName = "";

    out.offset = var->offset;
    out.size = var->size;

    if (var->type_description) {
        const SpvReflectTypeDescription* type_desc = var->type_description;
        
        if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) {
            out.type = UniformType::NestedStruct;
        }
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_MATRIX) {
            if (type_desc->traits.numeric.matrix.column_count == 2 && 
                type_desc->traits.numeric.matrix.row_count == 2) {
                out.type = UniformType::Mat2;
            } else if (type_desc->traits.numeric.matrix.column_count == 3 && 
                       type_desc->traits.numeric.matrix.row_count == 3) {
                out.type = UniformType::Mat3;
            } else if (type_desc->traits.numeric.matrix.column_count == 4 && 
                       type_desc->traits.numeric.matrix.row_count == 4) {
                out.type = UniformType::Mat4;
            } else {
                out.type = UniformType::Unknown;
            }
        }
        // Check for vector types
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR) {
            uint32_t component_count = type_desc->traits.numeric.vector.component_count;
            
            if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) {
                if (component_count == 2) out.type = UniformType::Vec2;
                else if (component_count == 3) out.type = UniformType::Vec3;
                else if (component_count == 4) out.type = UniformType::Vec4;
                else out.type = UniformType::Unknown;
            } else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_INT) {
                if (component_count == 2) out.type = UniformType::IVec2;
                else if (component_count == 3) out.type = UniformType::IVec3;
                else if (component_count == 4) out.type = UniformType::IVec4;
            } else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL) {
                out.type = UniformType::Bool;
            } else {
                out.type = UniformType::Unknown;
            }
        }
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) {
            out.type = UniformType::Float;
        }
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_INT) {
            if (type_desc->traits.numeric.scalar.signedness) {
                out.type = UniformType::Int;
            } else {
                out.type = UniformType::UInt;
            }
        }
        else if (type_desc->type_flags & SPV_REFLECT_TYPE_FLAG_BOOL) {
            out.type = UniformType::Bool;
        }
        else {
            out.type = UniformType::Unknown;
        }
    } else {
        out.type = UniformType::Unknown;
    }

    uint32_t dims_count = 0;
    const uint32_t* dims_ptr = nullptr;

    if (var->array.dims_count > 0) {
        dims_count = var->array.dims_count;
        dims_ptr = var->array.dims;
    } else if (var->type_description && var->type_description->traits.array.dims_count > 0) {
        dims_count = var->type_description->traits.array.dims_count;
        dims_ptr = var->type_description->traits.array.dims;
    }

    if (dims_count > 0 && dims_ptr) {
        out.isArray = true;
        out.arrayDims.assign(dims_ptr, dims_ptr + dims_count);
    } else {
        out.isArray = false;
    }

    if (var->member_count > 0 && var->members) {
        out.members.reserve(var->member_count);
        for (uint32_t m = 0; m < var->member_count; ++m) {
            UniformMember child;
            ParseBlockVariable(&var->members[m], child);
            out.members.push_back(std::move(child));
        }
    }
}

static Uniforms SpirvReflectUniforms(const std::string& spirv)
{
    size_t spirv_nbytes = spirv.size();
    const void* spirv_code = spirv.data();

    if (spirv_nbytes % sizeof(uint32_t) != 0) {
        PrintError("SPIR-V binary is corrupt or truncated");
        return {};
    }

    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(spirv_nbytes,
                                                           reinterpret_cast<const uint32_t*>(spirv_code),
                                                           &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        PrintError("Failed to create SPIR-V Reflect Shader Module");
        return {};
    }

    uint32_t binding_count = 0;
    result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    SpvReflectDescriptorBinding** bindings =
        static_cast<SpvReflectDescriptorBinding**>(malloc(binding_count * sizeof(SpvReflectDescriptorBinding*)));

    if (bindings == NULL) {
        spvReflectDestroyShaderModule(&module);
        PrintError("Failed to allocate memory for SPIR-V Reflect Descriptor Bindings");
        return {};
    }

    result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    Uniforms uniforms;
    uniforms.reserve(binding_count);

    for (uint32_t i = 0; i < binding_count; ++i)
    {
        const SpvReflectDescriptorBinding* binding = bindings[i];
        Uniform u;

        u.name = binding->name ? binding->name : "";
        u.set = binding->set;
        u.binding = binding->binding;
        u.size = binding->block.size;

        switch (binding->descriptor_type) {
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                if (u.name.empty() && binding->type_description) {
                    u.name = binding->type_description->type_name ? binding->type_description->type_name : u.name;
                }
                u.type = UniformType::NestedStruct;

                if (binding->block.member_count > 0 && binding->block.members) {
                    u.members.reserve(binding->block.member_count);
                    for (uint32_t m = 0; m < binding->block.member_count; ++m) {
                        UniformMember mem;
                        ParseBlockVariable(&binding->block.members[m], mem);
                        u.members.push_back(std::move(mem));
                    }
                }
                break;

            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                u.type = UniformType::Sampler2D;
                break;

            default:
                u.type = UniformType::Unknown;
                break;
        }

        if (u.type != UniformType::Unknown) {
            uniforms[u.name] = std::move(u);
        }
    }

    free(bindings);
    spvReflectDestroyShaderModule(&module);

    return uniforms;
}

static std::optional<PushConstant> SpirvReflectPushConstants(const std::string& spirv)
{
    size_t spirv_nbytes = spirv.size();
    if (spirv_nbytes % sizeof(uint32_t) != 0) {
        PrintError("SPIR-V binary is corrupt or truncated");
        return {};
    }

    SpvReflectShaderModule module;
    SpvReflectResult result = spvReflectCreateShaderModule(
        spirv_nbytes,
        reinterpret_cast<const uint32_t*>(spirv.data()),
        &module);

    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        PrintError("Failed to create SPIR-V Reflect Shader Module");
        return {};
    }

    uint32_t pc_count = 0;
    result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, nullptr);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    if (pc_count == 0) {
        spvReflectDestroyShaderModule(&module);
        return {};
    }

    SpvReflectBlockVariable** pc_blocks =
        static_cast<SpvReflectBlockVariable**>(
            malloc(pc_count * sizeof(SpvReflectBlockVariable*)));

    if (!pc_blocks) {
        PrintError("Allocation failure");
        spvReflectDestroyShaderModule(&module);
        return {};
    }

    result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, pc_blocks);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    assert(pc_count == 1);

    PushConstant pc;
    const SpvReflectBlockVariable* block = pc_blocks[0];

    pc.name = block->name ? block->name : "";
    pc.size = block->size;

    if (block->member_count > 0 && block->members) {
        pc.members.reserve(block->member_count);
        for (uint32_t m = 0; m < block->member_count; m++) {
            UniformMember mem;
            ParseBlockVariable(&block->members[m], mem);
            pc.members.push_back(std::move(mem));
        }
    }

    free(pc_blocks);
    spvReflectDestroyShaderModule(&module);

    return pc;
}

Uniforms VulkanRenderer::GetUniforms(Shader* shader)
{
    VertexShader* vertex = shader->GetVertexShader();
    FragmentShader* frag = shader->GetFragmentShader();
    
    Uniforms uniforms;

    Uniforms result = {};
    result = SpirvReflectUniforms(vertex->GetContent());
    uniforms.reserve(result.size());
    for (auto& uniform : result | std::views::values)
    {
        uniform.shaderType = ShaderType::Vertex;
        uniforms[uniform.name] = uniform;
    }
    
    result = SpirvReflectUniforms(frag->GetContent());
    uniforms.reserve(uniforms.size() + result.size());
    for (auto& uniform : result | std::views::values)
    {
        uniform.shaderType = ShaderType::Fragment;
        uniforms[uniform.name] = uniform;
    }
    
    return uniforms;
}

PushConstants VulkanRenderer::GetPushConstants(Shader* shader)
{
    VertexShader* vertex = shader->GetVertexShader();
    FragmentShader* frag = shader->GetFragmentShader();
    
    PushConstants pushConstants;
    std::optional<PushConstant> pushConstant = SpirvReflectPushConstants(vertex->GetContent());
    if (pushConstant.has_value())
    {
        pushConstant.value().shaderType = ShaderType::Vertex;
        pushConstants[ShaderType::Vertex] = pushConstant.value();
    }
    
    pushConstant = SpirvReflectPushConstants(frag->GetContent());
    if (pushConstant.has_value())
    {
        pushConstant.value().shaderType = ShaderType::Fragment;
        pushConstants[ShaderType::Fragment] = pushConstant.value();
    }
    return pushConstants;
}

void VulkanRenderer::SendTexture(UBOBinding binding, Texture* texture, Shader* shader)
{
    // VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    // auto descriptors = pipeline->GetDescriptorSets();
    // auto uniformBuffers = pipeline->GetUniformBuffers();
    // for (uint32_t j = 0; j < descriptors.size(); j++)
    // {
    //     for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    //     {
    //         descriptors[j]->UpdateDescriptorSets(i, j, shader->GetUniforms(), uniformBuffers, texture);
    //     }
    // }
}

void VulkanRenderer::SendValue(UBOBinding binding, void* value, uint32_t size, Shader* shader)
{
    // VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    //
    // VulkanUniformBuffer* uniformBuffer = pipeline->GetUniformBuffer(binding.set, binding.binding);
    //
    // uniformBuffer->WriteToMapped(value, size, m_currentFrame);
}

void VulkanRenderer::BindMaterial(Material* material)
{
    auto shader = material->GetShader();
    if (!shader)
        return;
    
    VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    pipeline->Bind(commandBuffer);

    material->Bind(this);

    // Set viewport and scissor dynamically
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapChain->GetExtent().width);
    viewport.height = static_cast<float>(m_swapChain->GetExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapChain->GetExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

std::unique_ptr<RHITexture> VulkanRenderer::CreateTexture(const ImageLoader::Image& image)
{
    std::unique_ptr<VulkanTexture> texture = std::make_unique<VulkanTexture>();
    texture->CreateFromImage(image, m_device.get(), m_commandPool.get(), m_device->GetGraphicsQueue());
    return texture;
}

std::unique_ptr<RHIVertexBuffer> VulkanRenderer::CreateVertexBuffer(const float* data, uint32_t size,
                                                                    uint32_t floatPerVertex)
{
    std::unique_ptr<VulkanVertexBuffer> vertexBuffer = std::make_unique<VulkanVertexBuffer>();

    VkDeviceSize bufferSize = sizeof(data[0]) * size;
    vertexBuffer->Initialize(m_device.get(), data, bufferSize, m_commandPool.get());
    vertexBuffer->SetVertexCount(size / floatPerVertex);

    return std::move(vertexBuffer);
}

std::unique_ptr<RHIIndexBuffer> VulkanRenderer::CreateIndexBuffer(const uint32_t* data, uint32_t size)
{
    std::unique_ptr<VulkanIndexBuffer> indexBuffer = std::make_unique<VulkanIndexBuffer>();

    VkDeviceSize bufferSize = sizeof(data[0]) * size;
    indexBuffer->Initialize(m_device.get(), data, bufferSize, VK_INDEX_TYPE_UINT32, m_commandPool.get());
    indexBuffer->SetIndexCount(size);

    return std::move(indexBuffer);
}

std::unique_ptr<RHIShaderBuffer> VulkanRenderer::CreateShaderBuffer(const std::string& code)
{
    std::unique_ptr<VulkanShaderBuffer> shaderBuffer = std::make_unique<VulkanShaderBuffer>();
    if (!shaderBuffer->Initialize(m_device.get(), code))
        return nullptr;
    return std::move(shaderBuffer);
}

std::unique_ptr<RHIPipeline> VulkanRenderer::CreatePipeline(const Shader* shader)
{
    std::unique_ptr<VulkanPipeline> pipeline = std::make_unique<VulkanPipeline>();
    pipeline->Initialize(m_device.get(), m_renderPass->GetRenderPass(), m_swapChain->GetExtent(), MAX_FRAMES_IN_FLIGHT, shader);
    return std::move(pipeline);
}
std::unique_ptr<RHIMaterial> VulkanRenderer::CreateMaterial(Shader* shader)
{
    VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    auto material = std::make_unique<VulkanMaterial>(pipeline);
    if (!material->Initialize(MAX_FRAMES_IN_FLIGHT, m_defaultTexture.get().get(), pipeline))
    {
        PrintError("Failed to initialize material from pipeline");
        return nullptr;
    }
    return std::move(material);
}

void VulkanRenderer::SetDefaultTexture(const SafePtr<Texture>& texture)
{
    if (!m_device)
    {
        PrintError("Failed to set default texture because device is not valid");
    }
    m_defaultTexture = texture.get();
    m_device->SetDefaultTexture(dynamic_cast<VulkanTexture*>(texture->GetBuffer()));
}

void VulkanRenderer::ClearColor() const
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    uint32_t imageIndex =  m_imageIndex;
    std::vector<VkClearValue> clearValues(2);
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[1].depthStencil = {.depth = 1.0f, .stencil = 0};
    
    m_renderPass->Begin(commandBuffer, m_framebuffer->GetFramebuffer(imageIndex),
                        m_swapChain->GetExtent(), clearValues);
}

void VulkanRenderer::RecreateSwapChain()
{
    Vec2i windowSize = m_window->GetSize();

    // Handle minimization
    while (windowSize.x == 0 || windowSize.y == 0)
    {
        windowSize = m_window->GetSize();
        m_window->WaitEvents();
    }

    vkDeviceWaitIdle(m_device->GetDevice());

    // Cleanup old swap chain resources
    m_framebuffer->Cleanup();
    m_swapChain->Cleanup();
    
    m_depthBuffer->Cleanup();

    // Recreate swap chain
    if (!m_swapChain->Initialize(m_device.get(), m_context->GetSurface(), m_window))
    {
        throw std::runtime_error("Failed to recreate swap chain!");
    }
    
    if (!m_depthBuffer->Initialize(m_device.get(), m_swapChain->GetExtent()))
    {
        throw std::runtime_error("Failed to recreate depth buffer!");
    }

    // Recreate framebuffers
    if (!m_framebuffer->Initialize(m_device.get(), m_renderPass->GetRenderPass(),
                                   m_swapChain->GetImageViews(),
                                   m_swapChain->GetExtent(), m_depthBuffer.get()))
    {
        throw std::runtime_error("Failed to recreate framebuffers!");
    }
}

#endif
