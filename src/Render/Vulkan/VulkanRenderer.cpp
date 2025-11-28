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

#include "VulkanDepthBuffer.h"
#include "VulkanDescriptorSetLayout.h"
#include "VulkanIndexBuffer.h"
#include "VulkanTexture.h"
#include "VulkanVertexBuffer.h"
#include "Debug/Log.h"
#include "Resource/Mesh.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"
#include "Utils/Type.h"

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

        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.pImmutableSamplers = nullptr;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        m_descriptorSetLayout = std::make_unique<VulkanDescriptorSetLayout>(m_device->GetDevice());
        m_descriptorSetLayout->Create(m_device->GetDevice(), {uboLayoutBinding, samplerLayoutBinding});

        m_pipeline = std::make_unique<VulkanPipeline>();
        if (!m_pipeline->Initialize(m_device.get(), m_renderPass->GetRenderPass(),
                                    m_swapChain->GetExtent(),
                                    "resources/shaders/shader.vert", "resources/shaders/shader.frag",
                                    {m_descriptorSetLayout->GetLayout()},
                                    {uboLayoutBinding, samplerLayoutBinding}))
        {
            PrintError("Failed to initialize pipeline!");
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

        m_uniformBuffer = std::make_unique<VulkanUniformBuffer>();
        if (!m_uniformBuffer->Initialize(m_device.get(), sizeof(UniformBufferObject), MAX_FRAMES_IN_FLIGHT))
        {
            PrintError("Failed to initialize uniform buffer!");
            return false;
        }
        m_uniformBuffer->MapAll();

        // Create Descriptor Pool
        m_descriptorPool = std::make_unique<VulkanDescriptorPool>();
        std::vector<VkDescriptorPoolSize> poolSizes = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT}
        };
        if (!m_descriptorPool->Initialize(m_device.get(), poolSizes, MAX_FRAMES_IN_FLIGHT))
        {
            PrintError("Failed to initialize descriptor pool!");
            return false;
        }

        // Create Descriptor Sets
        m_descriptorSet = std::make_unique<VulkanDescriptorSet>();
        if (!m_descriptorSet->Initialize(m_device.get(), m_descriptorPool->GetPool(),
                                         m_pipeline->GetDescriptorSetLayout(),
                                         MAX_FRAMES_IN_FLIGHT))
        {
            PrintError("Failed to initialize descriptor sets!");
            return false;
        }
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            m_descriptorSet->UpdateDescriptorSet(i, m_uniformBuffer.get(), nullptr);
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

        PrintLog("Vulkan renderer initialized successfully!");
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
    m_syncObjects.reset();
    m_commandPool.reset();
    m_descriptorSet.reset();
    m_descriptorPool.reset();
    m_uniformBuffer.reset();
    m_framebuffer.reset();
    m_depthBuffer.reset();
    m_pipeline.reset();
    m_renderPass.reset();
    m_descriptorSetLayout.reset();
    m_swapChain.reset();
    m_device.reset();
    m_context.reset();

    p_initialized = false;
    PrintLog("Vulkan renderer cleaned up");
}

void VulkanRenderer::SetModel(const SafePtr<Model>& model)
{
    m_model = model;
}

void VulkanRenderer::SetTexture(const SafePtr<Texture>& texture)
{
    m_texture = texture;
}

void VulkanRenderer::WaitUntilFrameFinished()
{
    m_syncObjects->WaitForFence(m_currentFrame);
}

void VulkanRenderer::Update()
{
    static auto lastTime = std::chrono::high_resolution_clock::now();
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

    m_uniformBuffer->WriteToMapped(&ubo, sizeof(ubo), m_currentFrame);
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
    
    return true;
}

void VulkanRenderer::DrawFrame()
{
    RecordCommandBuffer(m_commandPool->GetCommandBuffer(m_currentFrame), m_imageIndex);
}

void VulkanRenderer::EndFrame()
{
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

std::unique_ptr<RHITexture> VulkanRenderer::CreateTexture(const ImageLoader::Image& image)
{
    std::unique_ptr<VulkanTexture> texture = std::make_unique<VulkanTexture>();
    texture->CreateFromImage(image, m_device.get(), m_commandPool.get(), m_device->GetGraphicsQueue());

    // Update descriptor sets with the texture
    if (texture)
    {
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            // Update uniform buffer descriptor
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffer->GetBuffer(i);
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            // Update texture sampler descriptor
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture->GetImageView();
            imageInfo.sampler = texture->GetSampler();

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = m_descriptorSet->GetDescriptorSet(i);
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = m_descriptorSet->GetDescriptorSet(i);
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(m_device->GetDevice(),
                                   static_cast<uint32_t>(descriptorWrites.size()),
                                   descriptorWrites.data(), 0, nullptr);
        }
    }

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

void VulkanRenderer::SetDefaultTexture(const SafePtr<Texture>& texture)
{
    if (!m_device)
    {
        PrintError("Failed to set default texture because device is not valid");
    }
    m_device->SetDefaultTexture(dynamic_cast<VulkanTexture*>(texture->GetBuffer()));
}

void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    std::scoped_lock lock(m_commandPool->GetMutex());
    // Begin render pass
    std::vector<VkClearValue> clearValues(2);
    clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
    clearValues[1].depthStencil = {.depth = 1.0f, .stencil = 0};
    m_renderPass->Begin(commandBuffer, m_framebuffer->GetFramebuffer(imageIndex),
                        m_swapChain->GetExtent(), clearValues);

    // Bind pipeline
    m_pipeline->Bind(commandBuffer);

    // Bind descriptor sets
    VkDescriptorSet descriptorSet = m_descriptorSet->GetDescriptorSet(m_currentFrame);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            m_pipeline->GetPipelineLayout(), 0, 1,
                            &descriptorSet, 0, nullptr);

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

    // Draw the model if available
    if (m_model && m_model->IsLoaded() && m_model->SentToGPU())
    {
        auto meshes = m_model->GetMeshes();

        for (auto* mesh : meshes)
        {
            if (!mesh || !mesh->GetVertexBuffer() || !mesh->GetIndexBuffer())
                continue;

            VulkanVertexBuffer* vertexBuffer = static_cast<VulkanVertexBuffer*>(mesh->GetVertexBuffer());
            VulkanIndexBuffer* indexBuffer = static_cast<VulkanIndexBuffer*>(mesh->GetIndexBuffer());

            // Bind vertex buffer
            VkBuffer vkVertexBuffer = vertexBuffer->GetBuffer();
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vkVertexBuffer, offsets);

            // Bind index buffer
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->GetBuffer(), 0, indexBuffer->GetIndexType());

            // Draw indexed
            vkCmdDrawIndexed(commandBuffer, indexBuffer->GetIndexCount(), 1, 0, 0, 0);
        }
    }

    // End render pass
    m_renderPass->End(commandBuffer);
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
    
    m_depthBuffer->Cleanup(); // Destroy old resources

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
