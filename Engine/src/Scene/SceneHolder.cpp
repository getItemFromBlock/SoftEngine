#include "SceneHolder.h"

#include "Scene.h"

SceneHolder::SceneHolder()
{
    m_currentScene = nullptr;
}

void SceneHolder::Initialize()
{
    m_currentScene = std::make_unique<Scene>();
}

void SceneHolder::Update(float deltaTime)
{
    if (!m_currentScene)
        return;
    
    m_currentScene->OnUpdate(deltaTime);
}

void SceneHolder::Render(RHIRenderer* renderer)
{
    if (!m_currentScene)
        return;
    
    m_currentScene->OnRender(renderer);
}
