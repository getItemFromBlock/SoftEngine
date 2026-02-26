#pragma once
#include <vector>

#include "Utils/Type.h"

class Material;
class Shader;
class LightComponent;

class LightManager
{
public:
    void SendLights(Material* material) const;
private:
    friend LightComponent;
    
    void AddLight(LightComponent* light);
    void RemoveLight(LightComponent* light);
    
private:
    std::vector<SafePtr<LightComponent>> m_lights;
};

