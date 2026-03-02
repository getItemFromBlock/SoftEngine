#pragma once
#include <vector>

#include "Utils/Type.h"

class Scene;
class Material;
class Shader;
class LightComponent;

class LightManager
{
public:
    LightManager(Scene* scene) : m_scene(scene) {}
    
    void SendLights(Material* material) const;
private:
    friend LightComponent;
    
    void AddLight(LightComponent* light);
    void RemoveLight(LightComponent* light);
    
private:
    Scene* m_scene = nullptr;
    std::vector<SafePtr<LightComponent>> m_lights;
};


