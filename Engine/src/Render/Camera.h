#pragma once
#include <memory>
#include <galaxymath/Maths.h>

#include "Component/TransformComponent.h"
#include "Physic/Frustum.h"

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
    Camera& operator=(const Camera& other);
    Camera(const Camera&);
    Camera(Camera&&) noexcept;
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

    float GetAspectRatio() const;
    void SetAspectRatio(float aspectRatio);

    void SetClearColor(const Vec4f& color);
    Vec4f GetClearColor() const;

    virtual TransformComponent* GetTransform() const;

    void UpdateFrustum();
    const Frustum& GetFrustum() const;

    void SetSkybox(SafePtr<CubeMap> skybox);
    SafePtr<CubeMap> GetSkybox() const;
    
    void RenderSkybox(VulkanRenderer* renderer) const;
private:
    std::shared_ptr<TransformComponent> m_transform;

protected:
    float p_fov = 70.f;
    float p_far = 1000.f;
    float p_near = 0.03f;
    float p_aspectRatio = 4.f / 3.f;
    Vec2i p_framebufferSize;

    Vec4f p_clearColor = Vec4f(70.f / 255.f, 70.f / 255.f, 70.f / 255.f, 1.00f);

    ViewMode p_viewMode = ViewMode::Perspective;
    
    Frustum p_frustum;
    
    SafePtr<CubeMap> m_skybox;
};
