#pragma once
#include <memory>
#include <galaxymath/Maths.h>

#include "Component/TransformComponent.h"
#include "Physic/Frustum.h"
#include "Vulkan/VulkanDepthBuffer.h"
#include "Vulkan/VulkanGBuffer.h"
#include "Vulkan/VulkanTexture.h"

class PostProcessShader;
class Shader;
class RenderTargetTexture;
class CubeMap;

struct ViewMode
{
    enum class Type
    {
        Perspective,
        Orthographic
    };

    static const char* to_cstr()
    {
        return "Perspective\0Orthographic";
    }
};

class Camera : public IDescribe
{
public:
    Camera();
    ~Camera() override;

    Mat4 GetViewMatrix() const;
    Mat4 GetProjectionMatrix() const;
    Mat4 GetOrthographicMatrix() const;
    Mat4 GetViewProjectionMatrix() const;

    void Describe(ClassDescriptor& descriptor) override;

    float GetFOV() const;
    void SetFOV(float fov);

    float GetFar() const;
    void SetFar(float far);

    float GetNear() const;
    void SetNear(float near);

    void SetRenderTargetSize(uint32_t width, uint32_t height);
    Vec2i GetRenderTargetSize() const;

    float GetAspectRatio() const;

    void SetClearColor(const Vec4f& color);
    Vec4f GetClearColor() const;

    ViewMode::Type GetViewMode() const { return p_viewMode; }
    void SetViewMode(ViewMode::Type viewMode);

    virtual TransformComponent* GetTransform() const;

    void UpdateFrustum();
    const Frustum& GetFrustum() const;

    void SetSkybox(const SafePtr<CubeMap>& skybox);
    SafePtr<CubeMap> GetSkybox() const;

    void AddPostProcessShader(const SafePtr<PostProcessShader>& shader);
    void RemovePostProcessShader(const SafePtr<PostProcessShader>& shader);
    void RemovePostProcessShader(int index);
    void SetPostProcessShaderAt(int32_t index, const SafePtr<PostProcessShader>& shader);
    void ClearPostProcessShaders();
    const std::vector<SafePtr<PostProcessShader>>& GetPostProcessShaders() const;
    bool IsPostProcessActive() const;
    void CleanupPostprocessRenderTarget();

    void InitializeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height);
    void CleanupRenderTarget();
    void HandleResize(VulkanRenderer* renderer);

    SafePtr<Texture> MakeGBufferTexture(SafePtr<Texture> texture, const GBufferAttachment& attachment,
                                        VkSampler sampler, uint32_t width, uint32_t height);

    SafePtr<RenderTargetTexture> GetRenderTarget() const;

    void Begin();
    void EndGeometry();
    void End();

    void BeginForwardPass() const;
    void EndForwardPass();

    void BeginRenderTarget(const RenderTargetTexture* rtt, bool clearAttachment = true) const;
    void EndRenderTarget(RenderTargetTexture* rtt);

    void RenderSkybox(VulkanRenderer* renderer) const;
    void RenderPostProcess(VulkanRenderer* renderer);

    void BlitToSwapchain(VulkanRenderer* renderer);

    SafePtr<Material> GetGBufferMaterial() const { return m_gBufferMaterial; }

public:
    Event<Vec2i> OnRenderTargetResized;

private:
    void BeginGBufferPass(RenderTargetTexture* rtt);
    void EndGBufferPass();
    void BeginCompositionPass(RenderTargetTexture* rtt);
    void EndCompositionPass(RenderTargetTexture* rtt);
    void DrawComposition(VulkanRenderer* renderer) const;

    void EnsurePostProcessResources();

protected:
    float p_fov = 70.f;
    float p_far = 1000.f;
    float p_near = 0.03f;

    Vec4f p_clearColor = Vec4f(70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.00f);

    ViewMode::Type p_viewMode = ViewMode::Type::Perspective;

    Frustum p_frustum;

    // Skybox
    SafePtr<CubeMap> m_skybox;
    SafePtr<Material> m_skyboxMaterial;

    // Post process
    SafePtr<Mesh> m_quad;
    std::vector<SafePtr<PostProcessShader>> m_postProcessShaders;
    std::vector<SafePtr<Material>> m_postProcessMaterials;
    SafePtr<RenderTargetTexture> m_postProcessRenderTargets[2];

    Vec2i p_requestedSize;
    Vec2i p_renderTargetSize;
    SafePtr<RenderTargetTexture> m_renderTarget;

    std::unique_ptr<VulkanGBuffer> m_gBuffer = nullptr;
    SafePtr<Material> m_gBufferMaterial;
    SafePtr<Material> m_compositionMaterial;
    SafePtr<Texture> m_positionTexture;
    SafePtr<Texture> m_normalTexture;
    SafePtr<Texture> m_albedoTexture;
    SafePtr<Texture> m_metallicRoughnessTexture;

private:
    std::shared_ptr<TransformComponent> m_transform;
};
