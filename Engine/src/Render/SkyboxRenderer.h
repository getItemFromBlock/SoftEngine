#pragma once
#include <galaxymath/Maths.h>

#include "Utils/Type.h"

class Mesh;
class Material;
class CubeMap;
class VulkanRenderer;

class SkyboxRenderer
{
public:
    SkyboxRenderer();
    ~SkyboxRenderer();
    
    void Initialize();
    
    void RenderSkybox(VulkanRenderer* renderer, const SafePtr<Material>& skybox, const Mat4& viewProjection) const;
    
private:
    SafePtr<Mesh> m_cubeMesh;
};
