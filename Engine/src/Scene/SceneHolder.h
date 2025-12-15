#pragma once
#include <memory>
#include "Scene/Scene.h"

class SceneHolder
{
public:
    SceneHolder();

    void Initialize();

    Scene* GetCurrentScene() const { return m_currentScene.get(); }
    
    void Update(float deltaTime);
    void Render(RHIRenderer* renderer);
    
private:
    std::unique_ptr<Scene> m_currentScene;
};
