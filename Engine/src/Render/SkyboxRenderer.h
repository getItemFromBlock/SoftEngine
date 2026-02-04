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
    
    void RenderSkybox(VulkanRenderer* renderer, SafePtr<CubeMap> skybox, const Mat4& viewProjection) const;
    
private:
    SafePtr<Material> m_material;
    SafePtr<Mesh> m_cubeMesh;
};
