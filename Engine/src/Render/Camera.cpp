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
    SetSkybox(Engine::Get()->GetResourceManager()->GetDefaultCubeMap());
}

Camera::~Camera()
{
    
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

void Camera::Describe(ClassDescriptor& descriptor)
{
    descriptor.AddFloat("FOV", p_fov).setter = [this](void* data) { SetFOV(*static_cast<float*>(data)); };
    descriptor.AddFloat("Near", p_near).setter = [this](void* data) { SetNear(*static_cast<float*>(data)); };
    descriptor.AddFloat("Far", p_far).setter = [this](void* data) { SetFar(*static_cast<float*>(data)); };
    descriptor.AddColor4("Clear Color", p_clearColor);
    descriptor.AddCubeMap("Skybox", m_skybox).setter = [this](void* data) { SetSkybox(*static_cast<SafePtr<CubeMap>*>(data)); };
    descriptor.AddShader("Post process", m_postProcessShader).setter = 
        [this](void* data)
        {
            SetPostProcessShader(*static_cast<SafePtr<Shader>*>(data));
        };
    // descriptor.AddEnum("View mode", p_viewMode).setter = [this](void* data) { SetViewMode(*static_cast<ViewMode*>(data)); };
    //TODO: Add view mode
}

float Camera::GetFOV() const
{
    return p_fov;
}

void Camera::SetFOV(float fov)
{
    p_fov = fov;
    GetTransform()->SetDirty();
}

float Camera::GetFar() const
{
    return p_far;
}

void Camera::SetFar(float far)
{
    p_far = far;
    GetTransform()->SetDirty();
}

float Camera::GetNear() const
{
    return p_near;
}

void Camera::SetNear(float near)
{
    p_near = near;
    GetTransform()->SetDirty();
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

TransformComponent* Camera::GetTransform() const
{
    return m_transform.get();
}

void Camera::UpdateFrustum()
{
    p_frustum.Create(this);
}

const Frustum& Camera::GetFrustum() const
{
    return p_frustum;
}

void Camera::SetSkybox(const SafePtr<CubeMap>& skybox)
{
    m_skybox = skybox;
    if (m_skybox && !m_skyboxMaterial)
    {
        ResourceManager* resourceManager = Engine::Get()->GetResourceManager();
        m_skyboxMaterial = resourceManager->CreateMaterial("Skybox Material");
        SafePtr<Shader> skyboxShader = resourceManager->Load<Shader>(RESOURCE_PATH"shaders/Skybox/skybox.shader");
        m_skyboxMaterial->SetShader(skyboxShader);
    }
    m_skyboxMaterial->SetAttribute("skyboxSampler", m_skybox);
}

SafePtr<CubeMap> Camera::GetSkybox() const
{
    return m_skybox;
}

void Camera::SetPostProcessShader(const SafePtr<Shader>& shader)
{
    m_postProcessShader = shader;
    if (!m_postProcessMaterial && shader)
    {
        auto resourceManager = Engine::Get()->GetResourceManager();
        m_postProcessMaterial = resourceManager->CreateMaterial("Post Process Material");
        
        m_quad = resourceManager->Load<Mesh>(RESOURCE_PATH"models/Plane.obj/Plane.mesh");
        std::shared_ptr<RenderTargetTexture> renderTarget = std::make_shared<RenderTargetTexture>("Editor Render Target Post Process");
        auto renderer = Engine::Get()->GetRenderer();
        renderTarget->CreateRenderTarget(renderer, p_renderTargetSize.x, p_renderTargetSize.y);
        
        m_postProcessRenderTarget = resourceManager->AddResource(renderTarget);
    }
    if (shader)
    {
        m_postProcessMaterial->SetShader(shader);
        m_postProcessMaterial->SetAttribute("albedoSampler", m_renderTarget);
    }
}

SafePtr<Shader> Camera::GetPostProcessShader() const
{
    return m_postProcessMaterial->GetShader();
}

bool Camera::IsPostProcessActive() const
{
    return m_postProcessShader.valid();
}

void Camera::InitializeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height)
{
    p_renderTargetSize = Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height));
    p_requestedSize = p_renderTargetSize;

    std::shared_ptr<RenderTargetTexture> renderTarget = std::make_shared<RenderTargetTexture>("Editor Render Target");
    renderTarget->CreateRenderTarget(renderer, width, height);
    
    m_renderTarget = Engine::Get()->GetResourceManager()->AddResource(renderTarget);

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
    if (m_postProcessRenderTarget)
    {
        m_postProcessRenderTarget->Resize(renderer, width, height);
    }
    OnRenderTargetResized.Invoke(Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height)));
    
    p_renderTargetSize = Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height));
    p_requestedSize = p_renderTargetSize;
    
    m_transform->SetDirty();
}

void Camera::CleanupRenderTarget()
{
    if (!m_renderTarget)
        return;

    if (VulkanRenderer* renderer = Engine::Get()->GetRenderer())
    {
        renderer->WaitForGPU();
    }
    
    Engine::Get()->GetResourceManager()->RemoveResource(m_renderTarget->GetUUID());
    m_renderTarget.reset();
}

SafePtr<RenderTargetTexture> Camera::GetRenderTarget() const
{
    return IsPostProcessActive() ? m_postProcessRenderTarget : m_renderTarget;
}

void Camera::Begin()
{
    UpdateResizeRenderTarget(Engine::Get()->GetRenderer());
    BeginRenderTarget(m_renderTarget.getPtr());
    m_firstFrame = false;
}

void Camera::End()
{
    EndRenderTarget(m_renderTarget.getPtr());
    RenderPostProcess(Engine::Get()->GetRenderer());
}

void Camera::UpdateResizeRenderTarget(VulkanRenderer* renderer)
{
    ResizeRenderTarget(renderer, p_requestedSize.x, p_requestedSize.y);
}

void Camera::BeginRenderTarget(RenderTargetTexture* rtt)
{
    if (!rtt)
        return;
    
    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer commandBuffer = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());
    
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = m_firstFrame ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rtt->GetBuffer()->GetImage();
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
                                    rtt->GetBuffer()->GetImageView(),
                                    rtt->GetDepthBuffer()->GetImageView(),
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
}

void Camera::EndRenderTarget(RenderTargetTexture* rtt)
{
    if (!rtt)
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
    barrier.image = rtt->GetBuffer()->GetImage();
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

void Camera::RenderPostProcess(VulkanRenderer* renderer)
{
    if (!m_postProcessMaterial)
        return;
    
    BeginRenderTarget(m_postProcessRenderTarget.getPtr());
    
    if (!m_postProcessMaterial.valid() || !m_quad.valid())
        return;
    if (!m_postProcessMaterial->SentToGPU() || !m_quad->SentToGPU())
        return;
    
    if (!renderer->BindShader(m_postProcessMaterial->GetShader().getPtr()))
        return;
    if (!renderer->BindMaterial(m_postProcessMaterial.getPtr()))
        return;
    m_postProcessMaterial->SendAllValues(renderer);
    renderer->BindVertexBuffers(m_quad->GetVertexBuffer(), m_quad->GetIndexBuffer());
    uint32_t startIndex = m_quad->GetSubMeshes()[0].startIndex;
    uint32_t indexCount = m_quad->GetSubMeshes()[0].count;
    renderer->DrawVertexSubMesh(m_quad->GetIndexBuffer(), 
                                startIndex, 
                                indexCount);
    EndRenderTarget(m_postProcessRenderTarget.getPtr());
}

void Camera::RenderSkybox(VulkanRenderer* renderer) const
{
    if (!m_skybox)
        return;
    
    Mat4 view = GetViewMatrix();
    view[3] = Vec3f::Zero();
    Mat4 proj = GetProjectionMatrix();
    renderer->GetSkyboxRenderer()->RenderSkybox(renderer, m_skyboxMaterial, proj * view);
}
