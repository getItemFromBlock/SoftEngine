#include "SceneHolder.h"

#include "Scene.h"

SceneHolder::SceneHolder()
{
    m_currentScene = nullptr;
}

SceneHolder::~SceneHolder()
{
    if (m_currentScene)
        m_currentScene.reset();
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

void SceneHolder::Render(VulkanRenderer* renderer)
{
    if (!m_currentScene)
        return;
    
    m_currentScene->OnRender(renderer);
}
