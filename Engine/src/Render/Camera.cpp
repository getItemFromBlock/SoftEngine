#include "Camera.h"

#include "Component/TransformComponent.h"
#include "Core/Engine.h"
#include "Vulkan/VulkanRenderer.h"

#undef far
#undef near

Camera::Camera()
{
    m_transform = std::make_shared<TransformComponent>();
    m_skybox = Engine::Get()->GetResourceManager()->GetDefaultCubeMap();
}

Camera& Camera::operator=(const Camera& other) = default;
Camera::Camera(const Camera&) = default;
Camera::Camera(Camera&&) noexcept = default;
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

float Camera::GetAspectRatio() const
{
    return p_aspectRatio;
}

void Camera::SetAspectRatio(float aspectRatio)
{
    p_aspectRatio = aspectRatio;
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
    Mat4 projection = Mat4::CreateProjectionMatrix(p_fov, p_aspectRatio, p_near, p_far);
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
