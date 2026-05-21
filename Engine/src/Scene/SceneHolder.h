#pragma once
#include <filesystem>
#include <memory>

#include "Scene/Scene.h"

class SceneHolder
{
public:
    SceneHolder();
    ~SceneHolder();

    void Initialize();
    void Update();

    Scene* GetCurrentScene() const { return m_currentScene.get(); }
    const std::filesystem::path& GetScenePath() const { return m_scenePath; }
    void SetScenePath(const std::filesystem::path& path) { m_scenePath = path; }
    
    void BeginPlay();
    void EndPlay();
    
    bool SaveCurrentScene(const std::filesystem::path& path = {}, bool updateScenePath = true) const;
    bool LoadScene(const std::filesystem::path& path, bool updateScenePath = true);
    void NewScene();
    
    void PreFrame(VulkanRenderer* renderer);
    void UpdateEditor(float deltaTime);
    void UpdateRuntime(float deltaTime);
    void Render(VulkanRenderer* renderer);
    
private:
    std::unique_ptr<Scene> m_currentScene;
    std::filesystem::path m_scenePath = "Engine/scenes/scene.json";
    std::filesystem::path m_playModeSnapshotPath = "Engine/cache/playmode.scene.json";
    bool m_hasStartedRuntime = false;
    
    bool m_shouldCreateNewScene = false;
};
