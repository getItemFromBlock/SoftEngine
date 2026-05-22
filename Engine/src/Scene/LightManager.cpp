#include "LightManager.h"

#include "GameObject.h"
#include "Component/LightComponent.h"
#include "Resource/Material.h"
#include "Resource/Shader.h"

struct LightData
{
    Vec4f position;
    Vec4f color;
    Vec4f angles;
};

void LightManager::SendLights(Material* material) const
{
    if (m_lights.empty())
        return;
    
    material->SetAttribute("lightData.numLights", static_cast<int>(m_lights.size()));
    size_t i = 0;
    Vec4f data[4];
    for (const auto& light : m_lights)
    {
        light->SerializeData(data);
        material->SetAttribute("lightData.lights[" + std::to_string(i) + "].position", data[0]);
        material->SetAttribute("lightData.lights[" + std::to_string(i) + "].direction", data[1]);
        material->SetAttribute("lightData.lights[" + std::to_string(i) + "].angles", data[2]);
        material->SetAttribute("lightData.lights[" + std::to_string(i) + "].color", data[3]);
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

void LightManager::RemoveAllLights()
{
    m_lights.clear();
}

