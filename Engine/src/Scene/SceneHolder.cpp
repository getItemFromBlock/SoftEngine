#include "SceneHolder.h"

#include "Scene.h"

SceneHolder::SceneHolder()
{
    m_currentScene = nullptr;
}

SceneHolder::~SceneHolder()
{
    m_currentScene.reset();
}

void SceneHolder::Initialize()
{
    m_currentScene = std::make_unique<Scene>();
}

void SceneHolder::BeginPlay()
{
    m_hasStartedRuntime = false;
}

void SceneHolder::EndPlay()
{
    m_hasStartedRuntime = false;
}

void SceneHolder::PreFrame(VulkanRenderer* renderer)
{
    if (!m_currentScene)
        return;
    
    m_currentScene->PreFrame(renderer);
}

void SceneHolder::UpdateEditor(float deltaTime)
{
    if (!m_currentScene)
        return;
    
    m_currentScene->OnUpdateEditor(deltaTime);
}

void SceneHolder::UpdateRuntime(float deltaTime)
{
    if (!m_currentScene)
        return;

    if (!m_hasStartedRuntime)
    {
        m_currentScene->OnStart();
        m_hasStartedRuntime = true;
    }

    m_currentScene->OnUpdateRuntime(deltaTime);
}

void SceneHolder::Render(VulkanRenderer* renderer)
{
    if (!m_currentScene)
        return;
    
    m_currentScene->OnRender(renderer);
}
