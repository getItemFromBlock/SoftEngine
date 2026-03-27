#include "Camera.h"

#include <utility>

#include "Component/TransformComponent.h"
#include "Core/Engine.h"
#include "Resource/CubeMap.h"
#include "Resource/PostProcessShader.h"
#include "Resource/RenderTargetTexture.h"
#include "Vulkan/VulkanRenderer.h"
#include "Vulkan/VulkanUtils.h"

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
    Mat4 orthographicMatrix = Mat4::CreateOrthographicMatrix(-10.f, 10.f, 10.f, -10.f, p_far, p_near);
    return orthographicMatrix;
}

Mat4 Camera::GetViewProjectionMatrix() const
{
    return (p_viewMode == ViewMode::Type::Perspective ? GetProjectionMatrix() : GetOrthographicMatrix()) *
        GetViewMatrix();
}

void Camera::Describe(ClassDescriptor& descriptor)
{
    GetTransform()->Describe(descriptor);
    descriptor.AddFloat("FOV", p_fov).setter = [this](void* data) { SetFOV(*static_cast<float*>(data)); };
    descriptor.AddFloat("Near", p_near).setter = [this](void* data) { SetNear(*static_cast<float*>(data)); };
    descriptor.AddFloat("Far", p_far).setter = [this](void* data) { SetFar(*static_cast<float*>(data)); };
    descriptor.AddColor4("Clear Color", p_clearColor);
    descriptor.AddCubeMap("Skybox", m_skybox).setter = [this](void* data)
    {
        SetSkybox(*static_cast<SafePtr<CubeMap>*>(data));
    };

    // PostProcess
    {
        Property property;
        property.data = &m_postProcessShaders;
        property.type = PropertyType::PostProcessShader;
        property.name = "Postprocess";
        property.isList = true;
        property.addElement = [this]()
        {
            auto renderer = Engine::Get()->GetRenderer();
            renderer->AddAfterRenderCallback([this]()
            {
                auto resourceManager = Engine::Get()->GetResourceManager();
                auto defaultpp = resourceManager->Load<PostProcessShader>(
                    RESOURCE_PATH"/shaders/PostProcess/inverted.pshader");
                AddPostProcessShader(defaultpp);
            });
        };
        property.removeElement = [this](int index)
        {
            auto renderer = Engine::Get()->GetRenderer();
            renderer->AddAfterRenderCallback([this, index]()
            {
                RemovePostProcessShader(index);
            });
        };
        property.setElement = [this](int index, void* data)
        {
            auto renderer = Engine::Get()->GetRenderer();
            auto pp = *static_cast<SafePtr<PostProcessShader>*>(data);
            renderer->AddAfterRenderCallback([this, index, pp]()
            {
                SetPostProcessShaderAt(index, pp);
            });
        };
        descriptor.AddProperty(property);
    }

    // PostProcess Material
    {
        for (const SafePtr<Material>& material : m_postProcessMaterials)
        {
            if (material.valid())
                material->Describe(descriptor);
        }
    }

    descriptor.AddEnum("View mode", reinterpret_cast<int32_t*>(&p_viewMode), ViewMode::to_cstr()).setter = [this
        ](void* data)
        {
            p_viewMode = *static_cast<ViewMode::Type*>(data);
            SetViewMode(p_viewMode);
        };
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
    if (width == 0 || height == 0)
        return;
    p_requestedSize = Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height));

    auto renderer = Engine::Get()->GetRenderer();
    if (!renderer)
        return;
    if (p_requestedSize == p_renderTargetSize || p_requestedSize.x <= 0 || p_requestedSize.y <= 0)
        return;
    renderer->AddAfterRenderCallback([this, renderer]()
    {
        HandleResize(renderer);
    });
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

void Camera::SetViewMode(ViewMode::Type viewMode)
{
    p_viewMode = viewMode;
    GetTransform()->SetDirty();
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

void Camera::AddPostProcessShader(const SafePtr<PostProcessShader>& shader)
{
    if (!shader)
        return;

    EnsurePostProcessResources();

    auto resourceManager = Engine::Get()->GetResourceManager();
    std::string name = "Post Process Material " + std::to_string(m_postProcessMaterials.size());
    SafePtr<Material> mat = resourceManager->CreateMaterial(name, shader);

    m_postProcessShaders.push_back(shader);
    m_postProcessMaterials.push_back(mat);
}

void Camera::RemovePostProcessShader(const SafePtr<PostProcessShader>& shader)
{
    for (size_t i = 0; i < m_postProcessShaders.size(); ++i)
    {
        if (m_postProcessShaders[i].getPtr() == shader.getPtr())
        {
            RemovePostProcessShader(static_cast<int>(i));
            return;
        }
    }
}

void Camera::RemovePostProcessShader(int index)
{
    if (VulkanRenderer* renderer = Engine::Get()->GetRenderer())
        renderer->WaitForGPU();

    auto resourceManager = Engine::Get()->GetResourceManager();
    resourceManager->RemoveResource(m_postProcessMaterials[index]->GetUUID());
    m_postProcessShaders.erase(m_postProcessShaders.begin() + index);
    m_postProcessMaterials.erase(m_postProcessMaterials.begin() + index);

    if (m_postProcessShaders.empty())
        CleanupPostprocessRenderTarget();
}

void Camera::SetPostProcessShaderAt(int32_t index, const SafePtr<PostProcessShader>& shader)
{
    if (index < 0 || index >= static_cast<int32_t>(m_postProcessShaders.size()))
        return;

    if (!shader)
    {
        RemovePostProcessShader(index);
        return;
    }

    m_postProcessShaders[index] = shader;
    m_postProcessMaterials[index]->SetShader(shader);
}

void Camera::ClearPostProcessShaders()
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    for (auto& mat : m_postProcessMaterials)
        resourceManager->RemoveResource(mat->GetUUID());

    m_postProcessShaders.clear();
    m_postProcessMaterials.clear();

    CleanupPostprocessRenderTarget();
}

const std::vector<SafePtr<PostProcessShader>>& Camera::GetPostProcessShaders() const
{
    return m_postProcessShaders;
}

bool Camera::IsPostProcessActive() const
{
    return !m_postProcessShaders.empty();
}

void Camera::InitializeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height)
{
    p_renderTargetSize = Vec2i(static_cast<int32_t>(width), static_cast<int32_t>(height));
    p_requestedSize = p_renderTargetSize;

    std::shared_ptr<RenderTargetTexture> renderTarget = std::make_shared<RenderTargetTexture>("Editor Render Target");
    renderTarget->CreateRenderTarget(renderer, width, height, VK_FILTER_NEAREST);
    m_renderTarget = Engine::Get()->GetResourceManager()->AddResource(renderTarget);

    {
        m_gBuffer = std::make_unique<VulkanGBuffer>();
        if (!m_gBuffer->Initialize(renderer->GetDevice(), width, height))
        {
            PrintError("Camera: failed to initialize G-Buffer");
            return;
        }

        ResourceManager* rm = Engine::Get()->GetResourceManager();
        SafePtr<Shader> compShader = rm->Load<Shader>(RESOURCE_PATH"shaders/Deferred/composition.shader");
        m_compositionMaterial = rm->CreateMaterial("Camera Composition Material", compShader);
        m_gBufferMaterial = rm->CreateMaterial("Camera GBuffer Material");

        m_positionTexture = rm->AddResource(std::make_shared<Texture>("Position GBuffer Texture"));
        m_normalTexture = rm->AddResource(std::make_shared<Texture>("Normal GBuffer Texture"));
        m_albedoTexture = rm->AddResource(std::make_shared<Texture>("Albedo GBuffer Texture"));
        m_metallicRoughnessTexture = rm->AddResource(std::make_shared<Texture>("MetallicRoughness GBuffer Texture"));

        m_compositionMaterial->SetAttribute("gPosition",
                                            MakeGBufferTexture(m_positionTexture, m_gBuffer->GetPosition(),
                                                               m_gBuffer->GetSampler(), width, height));
        m_compositionMaterial->SetAttribute(
            "gNormal", MakeGBufferTexture(m_normalTexture, m_gBuffer->GetNormal(), m_gBuffer->GetSampler(), width,
                                          height));
        m_compositionMaterial->SetAttribute("gAlbedo", MakeGBufferTexture(
                                                m_albedoTexture, m_gBuffer->GetAlbedo(), m_gBuffer->GetSampler(), width,
                                                height));
        m_compositionMaterial->SetAttribute("gMetallicRoughnessAO", MakeGBufferTexture(
                                                m_metallicRoughnessTexture, m_gBuffer->GetMetallicRoughness(),
                                                m_gBuffer->GetSampler(), width, height));

        SafePtr<Shader> gBufferShader = rm->Load<Shader>(RESOURCE_PATH"shaders/Deferred/gBuffer.shader");
        m_gBufferMaterial->SetShader(gBufferShader);
    }
}

void Camera::CleanupRenderTarget()
{
    if (!m_renderTarget)
        return;

    if (VulkanRenderer* renderer = Engine::Get()->GetRenderer())
    {
        renderer->WaitForGPU();
    }

    m_gBuffer.reset();

    Engine::Get()->GetResourceManager()->RemoveResource(m_renderTarget->GetUUID());
    m_renderTarget.reset();
}

void Camera::CleanupPostprocessRenderTarget()
{
    auto resourceManager = Engine::Get()->GetResourceManager();

    for (int slot = 0; slot < 2; ++slot)
    {
        if (m_postProcessRenderTargets[slot])
        {
            resourceManager->RemoveResource(m_postProcessRenderTargets[slot]->GetUUID());
            m_postProcessRenderTargets[slot].reset();
        }
    }
}

void Camera::HandleResize(VulkanRenderer* renderer)
{
    /*
    if (p_requestedSize == p_renderTargetSize || p_requestedSize.x <= 0 || p_requestedSize.y <= 0)
        return;
    if (!m_gBufferMaterial || !m_gBufferMaterial->HasBeenSent())
        return;

    renderer->WaitForGPU();

    const uint32_t w = static_cast<uint32_t>(p_requestedSize.x);
    const uint32_t h = static_cast<uint32_t>(p_requestedSize.y);

    m_gBuffer->Resize(w, h);
    m_compositionMaterial->SetAttribute("gPosition",
                                        MakeGBufferTexture(m_positionTexture, m_gBuffer->GetPosition(),
                                                           m_gBuffer->GetSampler(), w, h));
    m_compositionMaterial->SetAttribute(
        "gNormal", MakeGBufferTexture(m_normalTexture, m_gBuffer->GetNormal(), m_gBuffer->GetSampler(), w, h));
    m_compositionMaterial->SetAttribute(
        "gAlbedo", MakeGBufferTexture(m_albedoTexture, m_gBuffer->GetAlbedo(), m_gBuffer->GetSampler(), w, h));
    m_compositionMaterial->SetAttribute("gMetallicRoughnessAO",
                                        MakeGBufferTexture(m_metallicRoughnessTexture,
                                                           m_gBuffer->GetMetallicRoughness(), m_gBuffer->GetSampler(),
                                                           w, h));

    m_renderTarget->Resize(renderer, w, h);

    for (const auto& m_postProcessRenderTarget : m_postProcessRenderTargets)
    {
        if (m_postProcessRenderTarget)
            m_postProcessRenderTarget->Resize(renderer, w, h);
    }

    // Re-bind albedoSampler on pass 0 in case the main RT moved.
    if (!m_postProcessMaterials.empty())
        m_postProcessMaterials[0]->SetAttribute("albedoSampler", m_renderTarget);

    PrintWarning("Camera::HandleResize - Resized render targets to %dx%d", w, h);

    p_renderTargetSize = p_requestedSize;
    p_requestedSize = Vec2i::Zero();
    */
    GetTransform()->SetDirty();
}

SafePtr<Texture> Camera::MakeGBufferTexture(SafePtr<Texture> texture, const GBufferAttachment& attachment,
                                            VkSampler sampler, uint32_t width, uint32_t height)
{
    texture->CreateFromBuffer(attachment, sampler, width, height);
    return texture;
}

SafePtr<RenderTargetTexture> Camera::GetRenderTarget() const
{
    if (!IsPostProcessActive())
        return m_renderTarget;

    int lastSlot = static_cast<int>(m_postProcessShaders.size() - 1) % 2;
    return m_postProcessRenderTargets[lastSlot];
}

void Camera::Begin()
{
    BeginGBufferPass(m_renderTarget.getPtr());
}

void Camera::EndGeometry()
{
    EndGBufferPass();

    BeginCompositionPass(m_renderTarget.getPtr());
    DrawComposition(Engine::Get()->GetRenderer());
    EndCompositionPass(m_renderTarget.getPtr());
}

void Camera::End()
{
    RenderPostProcess(Engine::Get()->GetRenderer());
}

void Camera::BeginForwardPass() const
{
    BeginRenderTarget(m_renderTarget.getPtr(), false);
}

void Camera::EndForwardPass()
{
    EndRenderTarget(m_renderTarget.getPtr());
}

void Camera::BeginRenderTarget(const RenderTargetTexture* rtt, bool clearAttachment) const
{
    if (!rtt)
        return;

    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer commandBuffer = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());

    if (rtt->GetDepthBuffer() && rtt->GetDepthBuffer()->NeedsTransition())
    {
        VulkanUtils::TransitionImageLayout(renderer->GetCommandPool(), renderer->GetDevice()->GetGraphicsQueue(),
                                           VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                           renderer->GetDevice(), rtt->GetDepthBuffer()->GetImage());
        rtt->GetDepthBuffer()->ValidateTransition();
    }

    // transition from SHADER_READ_ONLY to COLOR_ATTACHMENT
    VkImageMemoryBarrier colorBarrier{};
    colorBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    colorBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    colorBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorBarrier.image = rtt->GetBuffer()->GetImage();
    colorBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    colorBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    colorBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (clearAttachment)
    {
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr,
            1, &colorBarrier);
    }
    else
    {
        VkImageMemoryBarrier depthBarrier{};
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = rtt->GetDepthBuffer()->GetImage();
        depthBarrier.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
        depthBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
            | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        VkImageMemoryBarrier barriers[] = {colorBarrier, depthBarrier};
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0, 0, nullptr, 0, nullptr,
            2, barriers);
    }

    std::vector<VkClearValue> clearValues(2);
    clearValues[0].color = {{p_clearColor.x, p_clearColor.y, p_clearColor.z, p_clearColor.w}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkExtent2D extent = {static_cast<uint32_t>(p_renderTargetSize.x), static_cast<uint32_t>(p_renderTargetSize.y)};
    renderer->GetRenderPass()->Begin(commandBuffer,
                                     rtt->GetBuffer()->GetImageView(),
                                     rtt->GetDepthBuffer() ? rtt->GetDepthBuffer()->GetImageView() : nullptr,
                                     extent,
                                     clearAttachment ? clearValues : std::vector<VkClearValue>{},
                                     clearAttachment);

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
        0, 0, nullptr, 0, nullptr,
        1, &barrier);
}

void Camera::RenderPostProcess(VulkanRenderer* renderer)
{
    if (m_postProcessShaders.empty())
        return;

    if (!m_quad.valid())
        return;
    if (!m_quad->HasBeenSent())
        return;

    const size_t passCount = m_postProcessShaders.size();

    for (size_t i = 0; i < passCount; ++i)
    {
        SafePtr<Material>& mat = m_postProcessMaterials[i];

        if (!mat.valid() || !mat->HasBeenSent())
            continue;
        if (!renderer->BindShader(mat->GetShader().getPtr()))
            continue;

        // Determine which render target is the source and which is the dest.
        SafePtr<RenderTargetTexture> src = (i == 0)
                                               ? m_renderTarget
                                               : m_postProcessRenderTargets[(i - 1) % 2];
        SafePtr<RenderTargetTexture> dest = m_postProcessRenderTargets[i % 2];
        
        mat->SetAttribute("albedoSampler", src);
        mat->SetAttribute("params.resolution", Vec2f(p_renderTargetSize), true);

        BeginRenderTarget(dest.getPtr());

        if (!renderer->BindMaterial(mat.getPtr()))
        {
            EndRenderTarget(dest.getPtr());
            continue;
        }

        mat->SendAllValues(renderer);
        renderer->BindVertexBuffers(m_quad->GetVertexBuffer(), m_quad->GetIndexBuffer());
        uint32_t startIndex = m_quad->GetSubMeshes()[0].startIndex;
        uint32_t indexCount = m_quad->GetSubMeshes()[0].count;
        renderer->DrawVertexSubMesh(m_quad->GetIndexBuffer(), startIndex, indexCount);

        EndRenderTarget(dest.getPtr());
    }
}

void Camera::BlitToSwapchain(VulkanRenderer* renderer)
{
    SafePtr<RenderTargetTexture> source = GetRenderTarget();
    if (!source) return;

    VkCommandBuffer cmd = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());
    
    renderer->GetRenderPass()->End(cmd);
    renderer->SetBlittedToSwapchain(true);

    VkImage srcImage = source->GetBuffer()->GetImage();
    VkImage dstImage = renderer->GetSwapChain()->GetImages()[renderer->GetImageIndex()];

    VkImageMemoryBarrier srcBarrier{};
    srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    srcBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    srcBarrier.image = srcImage;
    srcBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    srcBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    VkImageMemoryBarrier dstBarrier{};
    dstBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    dstBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dstBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dstBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dstBarrier.image = dstImage;
    dstBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    dstBarrier.srcAccessMask = 0;
    dstBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkImageMemoryBarrier preBarriers[] = {srcBarrier, dstBarrier};
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 2, preBarriers);

    VkImageBlit blitRegion{};
    blitRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blitRegion.srcOffsets[0] = {0, 0, 0};
    blitRegion.srcOffsets[1] = {p_renderTargetSize.x, p_renderTargetSize.y, 1};
    blitRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blitRegion.dstOffsets[0] = {0, 0, 0};
    blitRegion.dstOffsets[1] = {
        static_cast<int32_t>(renderer->GetSwapChain()->GetExtent().width),
        static_cast<int32_t>(renderer->GetSwapChain()->GetExtent().height), 1
    };

    vkCmdBlitImage(cmd,
                   srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blitRegion, VK_FILTER_LINEAR);

    srcBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    srcBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    dstBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dstBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    dstBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dstBarrier.dstAccessMask = 0;

    VkImageMemoryBarrier postBarriers[] = {srcBarrier, dstBarrier};
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 2, postBarriers);
}

void Camera::BeginGBufferPass(RenderTargetTexture* rtt)
{
    if (!rtt || !m_gBuffer) return;

    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer commandBuffer = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());
    VkExtent2D extent = {
        static_cast<uint32_t>(p_renderTargetSize.x),
        static_cast<uint32_t>(p_renderTargetSize.y)
    };

    std::array<VkImage, 4> gBufferImages = {
        m_gBuffer->GetPosition().image,
        m_gBuffer->GetNormal().image,
        m_gBuffer->GetAlbedo().image,
        m_gBuffer->GetMetallicRoughness().image
    };

    std::vector<VkImageMemoryBarrier> barriers;
    for (VkImage img : gBufferImages)
    {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barriers.push_back(b);
    }

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(barriers.size()), barriers.data());

    if (rtt->GetDepthBuffer()->NeedsTransition())
    {
        VulkanUtils::TransitionImageLayout(renderer->GetCommandPool(),
                                           renderer->GetDevice()->GetGraphicsQueue(),
                                           VK_IMAGE_LAYOUT_UNDEFINED,
                                           VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                           renderer->GetDevice(), rtt->GetDepthBuffer()->GetImage());
        rtt->GetDepthBuffer()->ValidateTransition();
    }

    renderer->GetRenderPass()->BeginGBuffer(commandBuffer, m_gBuffer.get(), rtt->GetDepthBuffer()->GetImageView(),
                                            extent);

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

void Camera::EndGBufferPass()
{
    if (!m_gBuffer) return;

    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer commandBuffer = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());

    renderer->GetRenderPass()->EndGBuffer(commandBuffer, m_gBuffer.get());

    std::array<VkImage, 4> gBufferImages = {
        m_gBuffer->GetPosition().image,
        m_gBuffer->GetNormal().image,
        m_gBuffer->GetAlbedo().image,
        m_gBuffer->GetMetallicRoughness().image
    };

    std::vector<VkImageMemoryBarrier> barriers;
    for (VkImage img : gBufferImages)
    {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers.push_back(b);
    }

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr,
                         static_cast<uint32_t>(barriers.size()), barriers.data());
}

void Camera::BeginCompositionPass(RenderTargetTexture* rtt)
{
    if (!rtt) return;

    VulkanRenderer* renderer = Engine::Get()->GetRenderer();
    VkCommandBuffer commandBuffer = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());
    VkExtent2D extent = {
        static_cast<uint32_t>(p_renderTargetSize.x),
        static_cast<uint32_t>(p_renderTargetSize.y)
    };

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.srcAccessMask = 0;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rtt->GetBuffer()->GetImage();
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);

    renderer->GetRenderPass()->BeginComposition(
        commandBuffer,
        rtt->GetBuffer()->GetImageView(),
        extent);

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

void Camera::EndCompositionPass(RenderTargetTexture* rtt)
{
    if (!rtt) return;

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
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Camera::DrawComposition(VulkanRenderer* renderer) const
{
    if (!m_compositionMaterial.valid())
        return;

    if (!renderer->BindShader(m_compositionMaterial->GetShader().getPtr()))
        return;

    if (!renderer->BindMaterial(m_compositionMaterial.getPtr()))
        return;

    auto scene = Engine::Get()->GetSceneHolder()->GetCurrentScene();
    auto lightManager = scene->GetLightManager();
    auto skyBox = GetSkybox();
    m_compositionMaterial->SetAttribute("irradianceSampler", skyBox, false, CubeMap::SampleMode::Irradiance);
    m_compositionMaterial->SetAttribute("prefilteredSampler", skyBox, false, CubeMap::SampleMode::Prefilter);
    m_compositionMaterial->SetAttribute("brdfLut", skyBox->GetBRDFLutTexture());
    m_compositionMaterial->SetAttribute("lightData.cameraPos", Vec4f(scene->GetCameraData().position));
    lightManager->SendLights(m_compositionMaterial.getPtr());

    m_compositionMaterial->SendAllValues(renderer);

    VkCommandBuffer commandBuffer = renderer->GetCommandPool()->GetCommandBuffer(renderer->GetFrameIndex());
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void Camera::EnsurePostProcessResources()
{
    auto resourceManager = Engine::Get()->GetResourceManager();
    auto renderer = Engine::Get()->GetRenderer();

    if (!m_quad)
        m_quad = resourceManager->Load<Mesh>(RESOURCE_PATH"models/Plane.obj/Plane.mesh");

    for (int slot = 0; slot < 2; ++slot)
    {
        if (!m_postProcessRenderTargets[slot])
        {
            std::string name = "Post Process RT " + std::to_string(slot);
            auto rt = std::make_shared<RenderTargetTexture>(name);
            rt->CreateRenderTarget(renderer, p_renderTargetSize.x, p_renderTargetSize.y, VK_FILTER_LINEAR, false);
            m_postProcessRenderTargets[slot] = resourceManager->AddResource(rt);
        }
    }
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
