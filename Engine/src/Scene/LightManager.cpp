#include "LightManager.h"

#include "GameObject.h"
#include "Component/LightComponent.h"
#include "Resource/Material.h"
#include "Resource/Shader.h"

struct LightData
{
    Vec4f position;
    Vec4f color;
};

void LightManager::SendLights(Material* material) const
{
    if (m_lights.empty())
        return;
    
    material->SetAttribute("lightData.numLights", static_cast<int>(m_lights.size()));
    size_t i = 0;
    for (const auto& light : m_lights)
    {
        material->SetAttribute("lightData.lights[" + std::to_string(i) + "].position", Vec4f(light->GetGameObject()->GetTransform()->GetWorldPosition()));
        material->SetAttribute("lightData.lights[" + std::to_string(i) + "].color", Vec4f(light->GetColor(), light->GetIntensity()));
        i++;
    }
}

void LightManager::AddLight(LightComponent* light)
{
    Scene* scene = light->GetGameObject()->GetScene();
    SafePtr<LightComponent> lightComponent = scene->GetComponent<LightComponent>(light->GetGameObject());
    m_lights.push_back(lightComponent);
}

void LightManager::RemoveLight(LightComponent* light)
{
    size_t index = 0;
    auto it = std::ranges::remove_if(m_lights, [light, &index](const SafePtr<LightComponent>& i)
    {
        index++;
        return i.getPtr() == light;
    }).begin();
    
    if (it != m_lights.end())
    {
        m_lights.erase(it);
    }
}

