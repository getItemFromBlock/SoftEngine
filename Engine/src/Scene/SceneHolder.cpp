#include "SceneHolder.h"

#include "Scene.h"
#include "SceneSerializer.h"
#include "Debug/Log.h"

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

void SceneHolder::Update()
{
    if (m_shouldCreateNewScene)
    {
        Initialize();
        m_shouldCreateNewScene = false;
    }
}

void SceneHolder::BeginPlay()
{
    m_hasStartedRuntime = false;
    SaveCurrentScene(m_playModeSnapshotPath, false);
}

void SceneHolder::EndPlay()
{
    m_hasStartedRuntime = false;
    LoadScene(m_playModeSnapshotPath, false);
}

bool SceneHolder::SaveCurrentScene(const std::filesystem::path& path, bool updateScenePath) const
{
    if (!m_currentScene)
        return false;

    const std::filesystem::path target = path.empty() ? m_scenePath : path;
    if (target.empty())
        return false;

    const bool saved = SceneSerializer::Save(m_currentScene.get(), target);
    if (!saved)
        PrintError("Failed to save scene to %s", target.generic_string().c_str());

    if (saved && updateScenePath)
        const_cast<SceneHolder*>(this)->m_scenePath = target;

    return saved;
}

bool SceneHolder::LoadScene(const std::filesystem::path& path, bool updateScenePath)
{
    if (!m_currentScene)
        return false;

    const std::filesystem::path target = path.empty() ? m_scenePath : path;
    if (target.empty())
        return false;

    const bool loaded = SceneSerializer::Load(m_currentScene.get(), target);
    if (!loaded)
        PrintError("Failed to load scene from %s", target.generic_string().c_str());

    if (loaded && updateScenePath)
        m_scenePath = target;

    m_hasStartedRuntime = false;
    return loaded;
}

void SceneHolder::NewScene()
{
    m_shouldCreateNewScene = true;
}

void SceneHolder::PreFrame(VulkanRenderer* renderer)
{
    Update();
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
