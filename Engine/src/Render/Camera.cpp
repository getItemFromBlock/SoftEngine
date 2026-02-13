#include "Camera.h"

#include <utility>

#include "Component/TransformComponent.h"
#include "Core/Engine.h"
#include "Resource/RenderTargetTexture.h"
#include "Vulkan/VulkanRenderer.h"

#undef far
#undef near

Camera::Camera()
{
    m_transform = std::make_shared<TransformComponent>();
    m_skybox = Engine::Get()->GetResourceManager()->GetDefaultCubeMap();
}

Camera::~Camera() = default;

void Camera::SetFOV(float fov)
{
    p_fov = fov;
}

float Camera::GetFar() const
{
    return p_far;
}

void Camera::SetFar(float far)
{
    p_far = far;
}

float Camera::GetNear() const
{
    return p_near;
}

void Camera::SetNear(float near)
{
    p_near = near;
}

void Camera::SetRenderTargetSize(uint32_t width, uint32_t height)
{
    p_requestedSize = Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height));
}

Vec2i Camera::GetRenderTargetSize() const
{
    return p_renderTargetSize;
}

float Camera::GetAspectRatio() const
{
    return static_cast<float>(p_renderTargetSize.x) / static_cast<float>(p_renderTargetSize.y);
}

void Camera::SetClearColor(const Vec4f& color)
{
    p_clearColor = color;
}

Vec4f Camera::GetClearColor() const
{
    return p_clearColor;
}

const Frustum& Camera::GetFrustum() const
{
    return p_frustum;
}

void Camera::SetSkybox(SafePtr<CubeMap> skybox)
{
    m_skybox = skybox;
}

SafePtr<CubeMap> Camera::GetSkybox() const
{
    return m_skybox;
}

void Camera::InitializeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height)
{
    p_renderTargetSize = Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height));
    p_requestedSize = p_renderTargetSize;

    std::shared_ptr<RenderTargetTexture> renderTarget = std::make_shared<RenderTargetTexture>("Editor Render Target");
    renderTarget->CreateRenderTarget(renderer, width, height);
    
    m_renderTarget = Engine::Get()->GetResourceManager()->AddResource(renderTarget);
    
    m_useRenderTarget = true;
    m_firstFrame = true;
}

void Camera::ResizeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height)
{
    if (std::cmp_equal(p_renderTargetSize.x, width) && std::cmp_equal(p_renderTargetSize.y, height) || width == 0 || height == 0)
        return;
        
    if (!m_renderTarget)
    {
        InitializeRenderTarget(renderer, width, height);
        return;
    }
    
    renderer->WaitForGPU();
    
    m_renderTarget->Resize(renderer, width, height);
    OnRenderTargetResized.Invoke(Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height)));
    
    p_renderTargetSize = Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height));
    p_requestedSize = p_renderTargetSize;
    
    m_transform->SetDirty();
}

void Camera::CleanupRenderTarget()
{
    if (!m_renderTarget)
        return;
    
    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    if (renderer)
    {
        renderer->WaitForGPU();
    }
    
    Engine::Get()->GetResourceManager()->RemoveResource(m_renderTarget->GetUUID());
    m_renderTarget.reset();
    
    m_useRenderTarget = false;
}

void Camera::BeginRenderTarget()
{
    if (!m_useRenderTarget || !m_renderTarget)
        return;
    
    ResizeRenderTarget(Engine::Get()->GetRenderer(), p_requestedSize.x, p_requestedSize.y);
    
    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer commandBuffer = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_firstFrame ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_renderTarget->GetBuffer()->GetImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = m_firstFrame ? 0 : VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        m_firstFrame ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    std::vector<VkClearValue> clearValues(2);
    clearValues[0].color = {{p_clearColor.x, p_clearColor.y, p_clearColor.z, p_clearColor.w}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkExtent2D extent = {static_cast<uint32_t>(p_renderTargetSize.x), static_cast<uint32_t>(p_renderTargetSize.y)};
    renderer->GetRenderPass()->Begin(commandBuffer, 
                                    m_renderTarget->GetBuffer()->GetImageView(),
                                    m_renderTarget->GetDepthBuffer()->GetImageView(),
                                    extent,
                                    clearValues);
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(p_renderTargetSize.x);
    viewport.height = static_cast<float>(p_renderTargetSize.y);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
    
    m_firstFrame = false;
}

void Camera::EndRenderTarget()
{
    if (!m_useRenderTarget || !m_renderTarget)
        return;
    
    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer commandBuffer = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());
    
    renderer->GetRenderPass()->End(commandBuffer);
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_renderTarget->GetBuffer()->GetImage();
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

SafePtr<RenderTargetTexture> Camera::GetRenderTarget() const
{
    return m_renderTarget;
}

bool Camera::IsUsingRenderTarget() const
{
    return m_useRenderTarget;
}

void Camera::RenderSkybox(VulkanRenderer* renderer) const
{
    if (!m_skybox)
        return;
    
    Mat4 view = GetViewMatrix();
    view[3] = Vec3f::Zero();
    Mat4 proj = GetProjectionMatrix();
    renderer->GetSkyboxRenderer()->RenderSkybox(renderer, m_skybox, proj * view);
}

Mat4 Camera::GetViewMatrix() const
{
    auto transform = GetTransform();
    return Mat4::LookAtRH(transform->GetLocalPosition(),
                          transform->GetLocalPosition() + transform->GetForward(),
                          transform->GetUp());
}

Mat4 Camera::GetProjectionMatrix() const
{
    Mat4 projection = Mat4::CreateProjectionMatrix(p_fov, GetAspectRatio(), p_near, p_far);
    projection[1][1] *= -1;
    return projection;
}

Mat4 Camera::GetOrthographicMatrix() const
{
    return Mat4::CreateOrthographicMatrix(-10.f, 10.f, -10.f, 10.f, p_near, p_far);
}

Mat4 Camera::GetViewProjectionMatrix() const
{
    return GetProjectionMatrix() * GetViewMatrix();
}

float Camera::GetFOV() const
{
    return p_fov;
}

TransformComponent* Camera::GetTransform() const
{
    return m_transform.get();
}

void Camera::UpdateFrustum()
{
    p_frustum.Create(this);
}