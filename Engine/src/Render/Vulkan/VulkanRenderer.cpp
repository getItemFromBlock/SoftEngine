#include "VulkanRenderer.h"

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

#include "VulkanDepthBuffer.h"
#include "VulkanDescriptorSetLayout.h"
#include "VulkanIndexBuffer.h"
#include "VulkanShaderBuffer.h"
#include "VulkanTexture.h"
#include "VulkanVertexBuffer.h"
#include "VulkanUtils.h"
#include "Core/Engine.h"

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
    m_renderQueueManager = std::make_unique<RenderQueueManager>();

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
        
        m_initialized = true;

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
    // Callbacks
    std::unordered_map<Core::UUID, std::function<void()>> callbacks;
    {
        std::scoped_lock lock(m_beforeRenderMutex);
        callbacks.swap(m_beforeRender);
    }

    for (auto& [id, method] : callbacks)
        method();
    
    callbacks.clear();
    {
        std::scoped_lock lock(m_afterRenderMutex);
        callbacks.swap(m_afterRender);
    }

    for (auto& [id, method] : callbacks)
        method();
    
    m_renderQueueManager->Cleanup();
    m_lineRenderer.Cleanup();
    
    m_syncObjects.reset();
    m_commandPool.reset();
    m_depthBuffer.reset();
    m_renderPass.reset();
    m_swapChain.reset();
    m_device.reset();
    m_context.reset();

    m_initialized = false;
    PrintLog("Vulkan renderer cleaned up");
}

void VulkanRenderer::WaitUntilFrameFinished()
{
    m_syncObjects->WaitForFence(m_currentFrame);
}

void VulkanRenderer::WaitForAllFrames()
{
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_syncObjects->WaitForFence(i);
    }
}

void VulkanRenderer::Update()
{
}

bool VulkanRenderer::BeginFrame()
{
    // Callbacks
    std::unordered_map<Core::UUID, std::function<void()>> callbacks;
    {
        std::scoped_lock lock(m_beforeRenderMutex);
        callbacks.swap(m_beforeRender);
    }

    for (auto& [id, method] : callbacks)
        method();
    
    p_triangleCount = 0;
    p_vertexCount = 0;
    p_chunkCount = 0;
    p_particleCount = 0;
    p_connectionCount = 0;
    p_totalMemory = 0;
    p_usedMemory = 0;
    m_imageIndex = 0;
    m_blittedToSwapchain = false;
    
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

void VulkanRenderer::IncrementParticleCount(uint64_t chunkCount, uint64_t particleCount, uint64_t connectionCount)
{
    p_chunkCount += chunkCount;
    p_particleCount += particleCount;
    p_connectionCount += connectionCount;
}

void VulkanRenderer::ReportMemoryUsage(uint64_t totalMemory, uint64_t usedMemory)
{
    p_totalMemory += totalMemory;
    p_usedMemory += usedMemory;
}

void VulkanRenderer::DrawFrame()
{
}

bool VulkanRenderer::MultiThreadSendToGPU()
{
#ifdef MULTI_THREAD
    return false;
#else
    return false;
#endif
}

void VulkanRenderer::EndFrame()
{
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);

    if (!m_blittedToSwapchain)
    {
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
    }

    auto& mutex = m_commandPool->GetMutex();
    mutex.unlock();
    m_commandPool->EndRecording(m_currentFrame);

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
        PrintError("Failed to submit draw command buffer!");
        return;
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
        PrintError("Failed to present swap chain image!");
        return;
    }

    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    
    // Callbacks
    std::unordered_map<Core::UUID, std::function<void()>> callbacks;
    {
        std::scoped_lock lock(m_afterRenderMutex);
        callbacks.swap(m_afterRender);
    }

    for (auto& [id, method] : callbacks)
        method();
}

void VulkanRenderer::SendPushConstants(void* data, uint32_t size, Shader* shader, PushConstant pushConstant) const
{
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    VulkanPipeline* pipeline = shader->GetPipeline();
    vkCmdPushConstants(commandBuffer, pipeline->GetPipelineLayout(),
                       pushConstant.shaderType == ShaderType::Vertex
                           ? VK_SHADER_STAGE_VERTEX_BIT
                           : VK_SHADER_STAGE_FRAGMENT_BIT,
                       pushConstant.offset, size, data);
}

void VulkanRenderer::BindVertexBuffers(VulkanVertexBuffer* vertexBuffer, VulkanIndexBuffer* indexBuffer) const
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);

    VkBuffer vkVertexBuffer = vertexBuffer->GetBuffer();
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vkVertexBuffer, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, indexBuffer->GetIndexType());
}

void VulkanRenderer::DrawVertex(VulkanVertexBuffer* vertexBuffer, const VulkanIndexBuffer* indexBuffer)
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);

    uint32_t indexCount = indexBuffer->GetIndexCount();
    
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    
    p_vertexCount += indexCount;
}

void VulkanRenderer::DrawVertexSubMesh(VulkanIndexBuffer* _indexBuffer, uint32_t startIndex, uint32_t indexCount)
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);

    vkCmdDrawIndexed(commandBuffer, indexCount, 1, startIndex, 0, 0);
    p_vertexCount += indexCount;
    p_triangleCount += indexCount / 3;
}

void VulkanRenderer::DrawInstanced(VulkanIndexBuffer* indexBuffer, VulkanVertexBuffer* vertexShader, VulkanBuffer* instanceBuffer, uint32_t instanceCount)
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    
    VkBuffer vertexBuffers[] = {vertexShader->GetBuffer(), instanceBuffer->GetBuffer()};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), static_cast<uint32_t>(instanceCount), 0, 0, 0);
    p_triangleCount += (indexBuffer->GetIndexCount() / 3) * instanceCount;
}

void VulkanRenderer::DrawInstanced(VulkanIndexBuffer *indexBuffer, VulkanVertexBuffer *vertexShader, uint32_t instanceCount)
{
    VkCommandBuffer commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);

    VkBuffer vertexBuffers[] = { vertexShader->GetBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), static_cast<uint32_t>(instanceCount), 0, 0, 0);
    p_triangleCount += (indexBuffer->GetIndexCount() / 3) * instanceCount;
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
        PrintError("Shader compilation failed: %s", module.GetErrorMessage().c_str());
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

    VulkanPipeline* pipeline = shader->GetPipeline();
    auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    pipeline->Bind(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

    return true;
}

bool VulkanRenderer::BindMaterial(Material* material)
{
    if (!material->Bind(this))
        return false;

    // auto commandBuffer = m_commandPool->GetCommandBuffer(m_currentFrame);
    // // Set viewport and scissor dynamically
    // VkViewport viewport{};
    // viewport.x = 0.0f;
    // viewport.y = 0.0f;
    // viewport.width = static_cast<float>(m_swapChain->GetExtent().width);
    // viewport.height = static_cast<float>(m_swapChain->GetExtent().height);
    // viewport.minDepth = 0.0f;
    // viewport.maxDepth = 1.0f;
    // vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    //
    // VkRect2D scissor{};
    // scissor.offset = {0, 0};
    // scissor.extent = m_swapChain->GetExtent();
    // vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    return true;
}

std::unique_ptr<VulkanTexture> VulkanRenderer::CreateTexture(const ImageLoader::Image& image, const TextureParam& param)
{
    std::unique_ptr<VulkanTexture> texture = std::make_unique<VulkanTexture>();
    texture->CreateFromImage(image, m_device.get(), m_commandPool.get(), m_device->GetGraphicsQueue(), param);
    return texture;
}

std::unique_ptr<VulkanTexture> VulkanRenderer::CreateCubeMap(const ImageLoader::HDRImage& image)
{
    std::unique_ptr<VulkanTexture> texture = std::make_unique<VulkanTexture>();
    texture->CreateCubemapFromHDR(image, m_device.get(), m_commandPool.get(), m_device->GetGraphicsQueue());
    return texture;
}

std::unique_ptr<VulkanTexture> VulkanRenderer::CreateCubeMapWithMips(int resolution, int mipLevels)
{
    std::unique_ptr<VulkanTexture> texture = std::make_unique<VulkanTexture>();
    texture->CreateCubemapWithMips(resolution, mipLevels, m_device.get(), m_commandPool.get(), m_device->GetGraphicsQueue());
    return texture;
}

std::unique_ptr<VulkanVertexBuffer> VulkanRenderer::CreateVertexBuffer(const float* data, uint32_t size,
                                                                       uint32_t floatPerVertex)
{
    std::unique_ptr<VulkanVertexBuffer> vertexBuffer = std::make_unique<VulkanVertexBuffer>();

    VkDeviceSize bufferSize = sizeof(data[0]) * size;
    vertexBuffer->Initialize(m_device.get(), data, bufferSize, m_commandPool.get());
    vertexBuffer->SetVertexCount(size / floatPerVertex);

    return std::move(vertexBuffer);
}

std::unique_ptr<VulkanIndexBuffer> VulkanRenderer::CreateIndexBuffer(const uint32_t* data, uint32_t size)
{
    std::unique_ptr<VulkanIndexBuffer> indexBuffer = std::make_unique<VulkanIndexBuffer>();

    VkDeviceSize bufferSize = sizeof(data[0]) * size;
    indexBuffer->Initialize(m_device.get(), data, bufferSize, VK_INDEX_TYPE_UINT32, m_commandPool.get());
    indexBuffer->SetIndexCount(size);

    return std::move(indexBuffer);
}

std::unique_ptr<VulkanShaderBuffer> VulkanRenderer::CreateShaderBuffer(const std::string& code)
{
    std::unique_ptr<VulkanShaderBuffer> shaderBuffer = std::make_unique<VulkanShaderBuffer>();
    if (!shaderBuffer->Initialize(m_device.get(), code))
        return nullptr;
    return std::move(shaderBuffer);
}

std::unique_ptr<VulkanPipeline> VulkanRenderer::CreatePipeline(const Shader* shader)
{
    std::unique_ptr<VulkanPipeline> pipeline = std::make_unique<VulkanPipeline>();
    pipeline->Initialize(m_device.get(), m_swapChain->GetExtent(), MAX_FRAMES_IN_FLIGHT, shader, 
                        m_renderPass->GetColorFormat(), m_renderPass->GetDepthFormat());
    return std::move(pipeline);
}

std::unique_ptr<VulkanMaterial> VulkanRenderer::CreateMaterial(Shader* shader)
{
    VulkanPipeline* pipeline = shader->GetPipeline();
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
    auto vulkanPipeline = shader->GetPipeline();
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
    m_device->SetDefaultTexture(texture->GetBuffer());
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

    if (m_depthBuffer->NeedsTransition())
    {
        VulkanUtils::TransitionImageLayout(m_commandPool.get(), m_device->GetGraphicsQueue(),
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            m_device.get(), m_depthBuffer->GetImage());
        m_depthBuffer->ValidateTransition();
    }

    m_renderPass->Begin(commandBuffer, 
                        m_swapChain->GetImageViews()[imageIndex], 
                        m_depthBuffer->GetImageView(), 
                        m_swapChain->GetExtent(), 
                        clearValues);
}

void VulkanRenderer::DrawLine(const Vec3f& start, const Vec3f& end, const Vec4f& color, float thickness)
{
    m_lineRenderer.AddLine(start, end, color, thickness);
}

void VulkanRenderer::DrawWireCube(const Vec3f& center, const Vec3f& size, const Vec4f& color, float thickness)
{
    // Define the eight vertices of the cube
    Vec3f vertices[8];
    vertices[0] = center + Vec3f(-size.x, -size.y, -size.z);
    vertices[1] = center + Vec3f(size.x, -size.y, -size.z);
    vertices[2] = center + Vec3f(size.x, size.y, -size.z);
    vertices[3] = center + Vec3f(-size.x, size.y, -size.z);
    vertices[4] = center + Vec3f(-size.x, -size.y, size.z);
    vertices[5] = center + Vec3f(size.x, -size.y, size.z);
    vertices[6] = center + Vec3f(size.x, size.y, size.z);
    vertices[7] = center + Vec3f(-size.x, size.y, size.z);

    // Draw the edges of the cube
    DrawLine(vertices[0], vertices[1], color, thickness);
    DrawLine(vertices[1], vertices[2], color, thickness);
    DrawLine(vertices[2], vertices[3], color, thickness);
    DrawLine(vertices[3], vertices[0], color, thickness);
    DrawLine(vertices[4], vertices[5], color, thickness);
    DrawLine(vertices[5], vertices[6], color, thickness);
    DrawLine(vertices[6], vertices[7], color, thickness);
    DrawLine(vertices[7], vertices[4], color, thickness);
    DrawLine(vertices[0], vertices[4], color, thickness);
    DrawLine(vertices[1], vertices[5], color, thickness);
    DrawLine(vertices[2], vertices[6], color, thickness);
    DrawLine(vertices[3], vertices[7], color, thickness);
}

Core::UUID VulkanRenderer::AddBeforeRenderCallback(const std::function<void()>& method, Core::UUID uuid)
{
    if (uuid == UUID_INVALID)
        uuid = {};
    m_beforeRenderMutex.lock();
    m_beforeRender[uuid] = method;
    m_beforeRenderMutex.unlock();
    return uuid;
}

Core::UUID VulkanRenderer::AddAfterRenderCallback(const std::function<void()>& method, Core::UUID uuid)
{
    if (uuid == UUID_INVALID)
        uuid = {};
    m_afterRenderMutex.lock();
    m_afterRender[uuid] = method;
    m_afterRenderMutex.unlock();
    return uuid;
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
