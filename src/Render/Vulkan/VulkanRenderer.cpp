#include "VulkanRenderer.h"
#ifdef RENDER_API_VULKAN

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderPass.h"
#include "VulkanPipeline.h"
#include "VulkanFramebuffer.h"
#include "VulkanCommandBuffer.h"
#include "VulkanSyncObjects.h"

#include "Core/Window.h"

#include <iostream>
#include <stdexcept>

VulkanRenderer::~VulkanRenderer() = default;

bool VulkanRenderer::Initialize(Window* window)
{
    if (!window)
    {
        std::cerr << "Invalid window pointer!" << std::endl;
        return false;
    }

    m_window = window;

    try
    {
        // Initialize Vulkan context
        m_context = std::make_unique<VulkanContext>();
        if (!m_context->Initialize(window))
        {
            std::cerr << "Failed to initialize Vulkan context!" << std::endl;
            return false;
        }

        m_device = std::make_unique<VulkanDevice>();
        if (!m_device->Initialize(m_context->GetInstance(), m_context->GetSurface()))
        {
            std::cerr << "Failed to initialize Vulkan device!" << std::endl;
            return false;
        }

        // Initialize swap chain
        m_swapChain = std::make_unique<VulkanSwapChain>();
        if (!m_swapChain->Initialize(m_device.get(), m_context->GetSurface(), window))
        {
            std::cerr << "Failed to initialize swap chain!" << std::endl;
            return false;
        }

        // Initialize render pass
        m_renderPass = std::make_unique<VulkanRenderPass>();
        if (!m_renderPass->Initialize(m_device.get(), m_swapChain->GetImageFormat()))
        {
            std::cerr << "Failed to initialize render pass!" << std::endl;
            return false;
        }

        // Initialize graphics pipeline
        m_pipeline = std::make_unique<VulkanPipeline>();
        if (!m_pipeline->Initialize(m_device.get(), m_renderPass->GetRenderPass(),
                                    m_swapChain->GetExtent(),
                                    "resources/shaders/vert.spv", "resources/shaders/frag.spv"))
        {
            std::cerr << "Failed to initialize pipeline!" << std::endl;
            return false;
        }

        // Initialize framebuffers
        m_framebuffer = std::make_unique<VulkanFramebuffer>();
        if (!m_framebuffer->Initialize(m_device.get(), m_renderPass->GetRenderPass(),
                                       m_swapChain->GetImageViews(),
                                       m_swapChain->GetExtent()))
        {
            std::cerr << "Failed to initialize framebuffers!" << std::endl;
            return false;
        }

        // Initialize command buffers
        m_commandBuffer = std::make_unique<VulkanCommandBuffer>();
        if (!m_commandBuffer->Initialize(m_device.get(), MAX_FRAMES_IN_FLIGHT))
        {
            std::cerr << "Failed to initialize command buffers!" << std::endl;
            return false;
        }

        // Initialize synchronization objects
        m_syncObjects = std::make_unique<VulkanSyncObjects>();
        if (!m_syncObjects->Initialize(m_device.get(), MAX_FRAMES_IN_FLIGHT))
        {
            std::cerr << "Failed to initialize sync objects!" << std::endl;
            return false;
        }

        m_initialized = true;

        window->EResizeEvent.Bind([this](Vec2i)
        {
            std::cout << "Window resized" << std::endl;
            m_framebufferResized = true;
            DrawFrame();
        });
        
        std::cout << "Vulkan renderer initialized successfully!" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Vulkan renderer initialization failed: " << e.what() << std::endl;
        Cleanup();
        return false;
    }
}

void VulkanRenderer::Cleanup()
{
    if (m_device)
    {
        vkDeviceWaitIdle(m_device->GetDevice());
    }

    m_syncObjects.reset();
    m_commandBuffer.reset();
    m_framebuffer.reset();
    m_pipeline.reset();
    m_renderPass.reset();
    m_swapChain.reset();
    if (m_device)
    {
        m_device->Cleanup();
        m_device.reset();
    }
    m_context.reset();

    m_initialized = false;
    std::cout << "Vulkan renderer cleaned up" << std::endl;
}

void VulkanRenderer::BeginFrame()
{
    // Wait for the previous frame to finish
    m_syncObjects->WaitForFence(m_currentFrame);
}

void VulkanRenderer::EndFrame()
{
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::DrawFrame()
{
    // std::cout << "Drawing frame" << std::endl;
    if (!m_initialized)
    {
        return;
    }

    // Wait for previous frame
    m_syncObjects->WaitForFence(m_currentFrame);

    // Acquire next image
    uint32_t imageIndex;
    VkResult result = m_swapChain->AcquireNextImage(
        m_syncObjects->GetImageAvailableSemaphore(m_currentFrame),
        &imageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // Reset fence only if we're submitting work
    m_syncObjects->ResetFence(m_currentFrame);

    // Record command buffer
    m_commandBuffer->Reset(m_currentFrame);
    m_commandBuffer->BeginRecording(m_currentFrame);
    RecordCommandBuffer(m_commandBuffer->GetCommandBuffer(m_currentFrame), imageIndex);
    m_commandBuffer->EndRecording(m_currentFrame);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { m_syncObjects->GetImageAvailableSemaphore(m_currentFrame) };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    VkCommandBuffer cmdBuffer = m_commandBuffer->GetCommandBuffer(m_currentFrame);
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkSemaphore signalSemaphores[] = { m_syncObjects->GetRenderFinishedSemaphore(m_currentFrame) };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    result = vkQueueSubmit(m_device->GetGraphicsQueue(), 1, &submitInfo,
                          m_syncObjects->GetInFlightFence(m_currentFrame));
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // Present
    result = m_swapChain->PresentImage(m_device->GetPresentQueue(), imageIndex,
                                      m_syncObjects->GetRenderFinishedSemaphore(m_currentFrame));

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

void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    // Begin render pass
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    m_renderPass->Begin(commandBuffer, m_framebuffer->GetFramebuffer(imageIndex),
                       m_swapChain->GetExtent(), clearColor);

    // Bind pipeline
    m_pipeline->Bind(commandBuffer);

    // Set viewport and scissor dynamically (optional, but good practice)
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

    // Draw a triangle (no vertex buffer needed, vertices hardcoded in shader)
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

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

    // Recreate swap chain
    if (!m_swapChain->Initialize(m_device.get(), m_context->GetSurface(), m_window))
    {
        throw std::runtime_error("Failed to recreate swap chain!");
    }

    // Recreate framebuffers
    if (!m_framebuffer->Initialize(m_device.get(), m_renderPass->GetRenderPass(),
                                   m_swapChain->GetImageViews(),
                                   m_swapChain->GetExtent()))
    {
        throw std::runtime_error("Failed to recreate framebuffers!");
    }
}

#endif