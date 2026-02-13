#pragma once
#include <memory>
#include <galaxymath/Maths.h>

#include "Component/TransformComponent.h"
#include "Physic/Frustum.h"
#include "Vulkan/VulkanDepthBuffer.h"
#include "Vulkan/VulkanTexture.h"

class Shader;
class RenderTargetTexture;
class CubeMap;

enum class ViewMode
{
    Perspective,
    Orthographic
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

    virtual TransformComponent* GetTransform() const;

    void UpdateFrustum();
    const Frustum& GetFrustum() const;

    void SetSkybox(const SafePtr<CubeMap>& skybox);
    SafePtr<CubeMap> GetSkybox() const;
    
    void SetPostProcessShader(const SafePtr<Shader>& shader);
    SafePtr<Shader> GetPostProcessShader() const;
    
    void InitializeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height);
    void ResizeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height);
    void CleanupRenderTarget();

    SafePtr<RenderTargetTexture> GetRenderTarget() const;
    
    void Begin();
    void End();

    void UpdateResizeRenderTarget(VulkanRenderer* renderer);
    void BeginRenderTarget(RenderTargetTexture* rtt);
    void EndRenderTarget(RenderTargetTexture* rtt);
    
    void RenderSkybox(VulkanRenderer* renderer) const;
    void RenderPostProcess(VulkanRenderer* renderer);

    Event<Vec2i> OnRenderTargetResized;

protected:
    float p_fov = 70.f;
    float p_far = 1000.f;
    float p_near = 0.03f;

    Vec4f p_clearColor = Vec4f(70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.00f);

    ViewMode p_viewMode = ViewMode::Perspective;
    
    Frustum p_frustum;
    
    // Skybox
    SafePtr<CubeMap> m_skybox;
    SafePtr<Material> m_skyboxMaterial;
    
    // Post process
    SafePtr<Mesh> m_quad;
    SafePtr<Material> m_postProcessMaterial;
    SafePtr<Shader> m_postProcessShader;
    SafePtr<RenderTargetTexture> m_postProcessRenderTarget;
    
    Vec2i p_requestedSize;
    Vec2i p_renderTargetSize;
    SafePtr<RenderTargetTexture> m_renderTarget;

private:
    std::shared_ptr<TransformComponent> m_transform;
    bool m_firstFrame = true;
};
