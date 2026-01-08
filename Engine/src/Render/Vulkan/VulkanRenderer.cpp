#include "VulkanRenderer.h"
#ifdef RENDER_API_VULKAN

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
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

#include "VulkanComputeDispatch.h"
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
#include "Resource/ComputeShader.h"
#include "Utils/SPVReflection.h"

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

        PrintLog("Vulkan renderer initialized successfully with dynamic rendering");
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
    if (m_imGuiPool != VK_NULL_HANDLE && m_device)
    {
        vkDestroyDescriptorPool(m_device->GetDevice(), m_imGuiPool, nullptr);
        m_imGuiPool = VK_NULL_HANDLE;
    }

    m_syncObjects.reset();
    m_commandPool.reset();
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
}

bool VulkanRenderer::BeginFrame()
{
    p_triangleCount = 0;
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
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    
    m_renderPass->End(commandBuffer);
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_swapChain->GetImages()[m_imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = 0;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

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

void VulkanRenderer::SendPushConstants(void* data, uint32_t size, Shader* shader, PushConstant pushConstant) const
{
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(),
                       pushConstant.shaderType == ShaderType::Vertex
                           ? VK_SHADER_STAGE_VERTEX_BIT
                           : VK_SHADER_STAGE_FRAGMENT_BIT,
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

void VulkanRenderer::DrawVertexSubMesh(RHIIndexBuffer* _indexBuffer, uint32_t startIndex, uint32_t indexCount)
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);

    vkCmdDrawIndexed(commandBuffer, indexCount, 1, startIndex, 0, 0);
    p_triangleCount += indexCount / 3;
}

void VulkanRenderer::DrawInstanced(RHIIndexBuffer* indexBuffer, RHIVertexBuffer* vertexShader, RHIBuffer* instanceBuffer, uint32_t instanceCount)
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    VulkanIndexBuffer* index = static_cast<VulkanIndexBuffer*>(indexBuffer);
    
    VkBuffer vertexBuffers[] = {Cast<VulkanVertexBuffer>(vertexShader)->GetBuffer(), Cast<VulkanBuffer>(instanceBuffer)->GetBuffer()};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, index->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffer, index->GetIndexCount(), static_cast<uint32_t>(instanceCount), 0, 0, 0);
    p_triangleCount += (index->GetIndexCount() / 3) * instanceCount;
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
    case ShaderType::Compute:
        kind = shaderc_compute_shader;
        break;
    default:
        PrintError("Invalid shader type");
        return "";
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;

    shaderc::SpvCompilationResult module =
        compiler.CompileGlslToSpv(code, kind, "shader.glsl", options);

    if (module.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        PrintError("Shader compilation failed: %s", module.GetErrorMessage().c_str());;
        return {};
    }

    std::vector<uint32_t> spirv(module.begin(), module.end());

    const char* begin = reinterpret_cast<const char*>(spirv.data());
    const char* end = begin + spirv.size() * sizeof(uint32_t);
    return std::string(begin, end);
}

Uniforms VulkanRenderer::GetUniforms(Shader* shader)
{
    VertexShader* vertex = shader->GetVertexShader();
    FragmentShader* frag = shader->GetFragmentShader();
    ComputeShader* comp = shader->GetComputeShader();

    Uniforms uniforms;

    Uniforms result = {};

    if (vertex)
    {
        result = SPV::SpirvReflectUniforms(vertex->GetContent());
        uniforms.reserve(result.size());
        for (auto& uniform : result | std::views::values)
        {
            uniform.shaderType = ShaderType::Vertex;
            uniforms[uniform.name] = uniform;
        }
    }

    if (frag)
    {
        result = SPV::SpirvReflectUniforms(frag->GetContent());
        uniforms.reserve(uniforms.size() + result.size());
        for (auto& uniform : result | std::views::values)
        {
            uniform.shaderType = ShaderType::Fragment;
            uniforms[uniform.name] = uniform;
        }
    }

    if (comp)
    {
        result = SPV::SpirvReflectUniforms(comp->GetContent());
        uniforms.reserve(uniforms.size() + result.size());
        for (auto& uniform : result | std::views::values)
        {
            uniform.shaderType = ShaderType::Compute;
            uniforms[uniform.name] = uniform;
        }
    }

    return uniforms;
}

PushConstants VulkanRenderer::GetPushConstants(Shader* shader)
{
    VertexShader* vertex = shader->GetVertexShader();
    FragmentShader* frag = shader->GetFragmentShader();
    ComputeShader* comp = shader->GetComputeShader();

    PushConstants pushConstants;
    std::optional<PushConstant> pushConstant;
    if (vertex)
    {
        pushConstant = SPV::SpirvReflectPushConstants(vertex->GetContent());
        if (pushConstant.has_value())
        {
            pushConstant.value().shaderType = ShaderType::Vertex;
            pushConstants[ShaderType::Vertex] = pushConstant.value();
        }
    }

    if (frag)
    {
        pushConstant = SPV::SpirvReflectPushConstants(frag->GetContent());
        if (pushConstant.has_value())
        {
            pushConstant.value().shaderType = ShaderType::Fragment;
            pushConstants[ShaderType::Fragment] = pushConstant.value();
        }
    }

    if (comp)
    {
        pushConstant = SPV::SpirvReflectPushConstants(comp->GetContent());
        if (pushConstant.has_value())
        {
            pushConstant.value().shaderType = ShaderType::Compute;
            pushConstants[ShaderType::Compute] = pushConstant.value();
        }
    }
    return pushConstants;
}

void VulkanRenderer::SendTexture(UBOBinding binding, Texture* texture, Shader* shader)
{
}

void VulkanRenderer::SendValue(UBOBinding binding, void* value, uint32_t size, Shader* shader)
{
}

bool VulkanRenderer::BindShader(Shader* shader)
{
    if (!shader || !shader->GetPipeline())
        return false;

    VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    pipeline->Bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

    return true;
}

bool VulkanRenderer::BindMaterial(Material* material)
{
    if (!material->Bind(this))
        return false;

    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
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

    return true;
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
    pipeline->Initialize(m_device.get(), m_swapChain->GetExtent(), MAX_FRAMES_IN_FLIGHT, shader, 
                        m_renderPass->GetColorFormat(), m_renderPass->GetDepthFormat());
    return std::move(pipeline);
}

std::unique_ptr<RHIMaterial> VulkanRenderer::CreateMaterial(Shader* shader)
{
    VulkanPipeline* pipeline = dynamic_cast<VulkanPipeline*>(shader->GetPipeline());
    auto material = std::make_unique<VulkanMaterial>(pipeline);
    if (!material->Initialize(MAX_FRAMES_IN_FLIGHT, m_defaultTexture.getPtr(), pipeline))
    {
        PrintError("Failed to initialize material from pipeline");
        return nullptr;
    }
    return std::move(material);
}

std::unique_ptr<ComputeDispatch> VulkanRenderer::CreateDispatch(Shader* shader)
{
    auto vulkanPipeline = Cast<VulkanPipeline>(shader->GetPipeline());
    std::unique_ptr<VulkanMaterial> material = std::make_unique<VulkanMaterial>(vulkanPipeline);
    if (!material->Initialize(MAX_FRAMES_IN_FLIGHT, m_defaultTexture.getPtr(), vulkanPipeline))
    {
        PrintError("Failed to initialize Compute Dispatch");
    }

    auto dispatch = std::make_unique<ComputeDispatch>();

    dispatch->SetMaterial(std::move(material));

    return std::move(dispatch);
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
    uint32_t imageIndex = m_imageIndex;
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_swapChain->GetImages()[imageIndex];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    std::vector<VkClearValue> clearValues(2);
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[1].depthStencil = {.depth = 1.0f, .stencil = 0};

    m_renderPass->Begin(commandBuffer, 
                       m_swapChain->GetImageViews()[imageIndex], 
                       m_depthBuffer->GetImageView(), 
                       m_swapChain->GetExtent(), 
                       clearValues);
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
}

#endif