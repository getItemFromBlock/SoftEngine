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
    auto firstLight = m_lights.back();
    LightData lightData;
    lightData.position = firstLight->GetGameObject()->GetTransform()->GetWorldPosition();
    lightData.color = firstLight->GetColor();
    
    material->SetAttribute("position", lightData.position);
    material->SetAttribute("color", lightData.color);
}

void LightManager::AddLight(LightComponent* light)
{
    Scene* scene = light->GetGameObject()->GetScene();
    SafePtr<LightComponent> lightComponent = scene->GetComponent<LightComponent>(light->GetGameObject());
    m_lights.push_back(lightComponent);
}

void LightManager::RemoveLight(LightComponent* light)
{
    auto it = std::ranges::remove_if(m_lights, [light](const SafePtr<LightComponent>& i)
    {
        return i.getPtr() == light;
    }).begin();
    if (it != m_lights.end())
    {
        m_lights.erase(it);
    }
}
