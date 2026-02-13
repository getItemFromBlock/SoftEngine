#pragma once
#include <memory>
#include <galaxymath/Maths.h>

#include "Component/TransformComponent.h"
#include "Physic/Frustum.h"
#include "Vulkan/VulkanDepthBuffer.h"
#include "Vulkan/VulkanTexture.h"

class RenderTargetTexture;
class CubeMap;

enum class ViewMode
{
    Perspective,
    Orthographic
};

class Camera
{
public:
    Camera();
    virtual ~Camera();

    Mat4 GetViewMatrix() const;
    Mat4 GetProjectionMatrix() const;
    Mat4 GetOrthographicMatrix() const;
    Mat4 GetViewProjectionMatrix() const;

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

    void SetSkybox(SafePtr<CubeMap> skybox);
    SafePtr<CubeMap> GetSkybox() const;
    
    void InitializeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height);
    void ResizeRenderTarget(VulkanRenderer* renderer, uint32_t width, uint32_t height);
    void CleanupRenderTarget();

    SafePtr<RenderTargetTexture> GetRenderTarget() const;
    bool IsUsingRenderTarget() const;
    
    void BeginRenderTarget();
    void EndRenderTarget();
    
    void RenderSkybox(VulkanRenderer* renderer) const;

    Event<Vec2i> OnRenderTargetResized;
private:
    std::shared_ptr<TransformComponent> m_transform;
    bool m_firstFrame = true;

protected:
    float p_fov = 70.f;
    float p_far = 1000.f;
    float p_near = 0.03f;

    Vec4f p_clearColor = Vec4f(70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.00f);

    ViewMode p_viewMode = ViewMode::Perspective;
    
    Frustum p_frustum;
    
    SafePtr<CubeMap> m_skybox;
    
    Vec2i p_requestedSize;
    Vec2i p_renderTargetSize;
    SafePtr<RenderTargetTexture> m_renderTarget;
    bool m_useRenderTarget = false;
};
