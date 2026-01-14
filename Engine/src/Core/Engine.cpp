#include "Engine.h"

#include "ThreadPool.h"
#include "Window.h"

#include "Debug/Log.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Resource/Mesh.h"
#include "Resource/Model.h"
#include "Resource/ResourceManager.h"

#include "Scene/Scene.h"

#include "Component/MeshComponent.h"
#include "Component/TestComponent.h"
#include "Component/TransformComponent.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"

Engine* Engine::Create()
{
    if (s_instance)
        return s_instance.get();
    s_instance = std::make_unique<Engine>();
    return s_instance.get();
}

bool Engine::Initialize(EngineDesc desc)
{
    m_window = desc.window;
    if (!m_window)
    {
        PrintError("No window provided");
        return false;
    }

    m_renderer = std::make_unique<VulkanRenderer>();
    m_renderer->Initialize(m_window);
    
    if (!m_renderer || !m_renderer->IsInitialized())
    {
        PrintError("Failed to create renderer");
        return false;
    }

    ThreadPool::Initialize();

    m_resourceManager = std::make_unique<ResourceManager>();
    m_resourceManager->Initialize(m_renderer.get());
    m_resourceManager->LoadDefaultTexture(RESOURCE_PATH"/textures/debug.jpeg");
    m_resourceManager->LoadBlankTexture(RESOURCE_PATH"/textures/blank.png");
    m_resourceManager->LoadDefaultShader(RESOURCE_PATH"/shaders/Unlit/unlit.shader");
    m_resourceManager->LoadDefaultMaterial(RESOURCE_PATH"/shaders/unlit.mat");
    
    // m_renderer->GetLineRenderer().Initialize(m_renderer.get());
    
    m_componentRegister = std::make_unique<ComponentRegister>();
    m_componentRegister->RegisterComponent<TransformComponent>();
    m_componentRegister->RegisterComponent<MeshComponent>();
    m_componentRegister->RegisterComponent<TestComponent>();
    
    m_sceneHolder = std::make_unique<SceneHolder>();
    m_sceneHolder->Initialize();
    return true;
}

bool Engine::BeginFrame()
{
    m_resourceManager->UpdateResourceToSend();
    m_renderer->WaitUntilFrameFinished();
    if (!m_renderer->BeginFrame())
        return false;

    return true;
}

void Engine::Update()
{
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    m_deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;
    
    m_sceneHolder->Update(m_deltaTime);
}

void Engine::Render()
{        
    m_renderer->ClearColor();
    
    m_renderer->AddLine(Vec3f(0, 0, 0), Vec3f(1, 1, 1), Vec4f(1, 0, 0, 1));

    m_sceneHolder->Render(m_renderer.get());
}

void Engine::EndFrame()
{
    m_renderer->EndFrame();
}

void Engine::WaitBeforeClean()
{
    // Wait for GPU to finish rendering before cleaning
    m_renderer->WaitForGPU();
    
    ThreadPool::WaitUntilAllTasksFinished();
}

void Engine::Cleanup()
{    
    m_sceneHolder.reset();
    m_resourceManager->CreateCache();
    m_resourceManager->Clear();
    m_renderer->Cleanup();
    
    ThreadPool::Terminate();

    m_window->Terminate();
}

Engine* Engine::Get()
{
    return s_instance.get();
}