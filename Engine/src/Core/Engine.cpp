#include "Engine.h"

#include "ThreadPool.h"
#include "Window.h"

#include "Debug/Log.h"

#include "Render/Vulkan/VulkanRenderer.h"

#include "Resource/Mesh.h"
#include "Resource/Model.h"
#include "Resource/ComputeShader.h"
#include "Resource/ResourceManager.h"

#include "Scene/Scene.h"
#include "Scene/GameObject.h"

#include "Component/MeshComponent.h"
#include "Component/TestComponent.h"
#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"
#include "Component/ParticleSystemComponent.h"
#include "Component/GPUSoftBodyComponent.h"


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
    
    // Create default resources
    {
        m_resourceManager->LoadDefaultTexture(RESOURCE_PATH"/textures/debug.jpeg");
        m_resourceManager->LoadBlankTexture(RESOURCE_PATH"/textures/blank.png");
        m_resourceManager->LoadBlackTexture(RESOURCE_PATH"/textures/black.png");
        m_resourceManager->LoadDefaultNormal(RESOURCE_PATH"/textures/defaultNormal.png");
        m_resourceManager->LoadDefaultCubeMap(RESOURCE_PATH"/envMap/newport_loft.hdr");
        m_resourceManager->LoadBlankCubeMap(RESOURCE_PATH"/envMap/blank.hdr");
        m_resourceManager->LoadDefaultShader(RESOURCE_PATH"/shaders/Deferred/gBuffer.shader");
        m_resourceManager->LoadDefaultMaterial(RESOURCE_PATH"/materials/pbr.mat");

        SafePtr<Shader> unlit = m_resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/Unlit/Unlit.shader");
        SafePtr<Material> mat = m_resourceManager->CreateMaterial(RESOURCE_PATH"/materials/unlit.mat");
        mat->SetAttribute("material.color", Vec4f::One());
        mat->SetShader(unlit);
    }
    m_resourceManager->InitializeResources();
    
    m_renderer->GetLineRenderer()->Initialize(m_renderer.get());
    m_renderer->GetSkyboxRenderer()->Initialize();
    
    m_componentRegister = std::make_unique<ComponentRegister>();
    m_componentRegister->RegisterComponent<TransformComponent>();
    m_componentRegister->RegisterComponent<MeshComponent>();
    m_componentRegister->RegisterComponent<TestComponent>();
    m_componentRegister->RegisterComponent<LightComponent>();
    m_componentRegister->RegisterComponent<ParticleSystemComponent>();
    m_componentRegister->RegisterComponent<GPUSoftBodyComponent>();
    
    m_sceneHolder = std::make_unique<SceneHolder>();
    m_sceneHolder->Initialize();
    return true;
}

bool Engine::BeginFrame()
{
    m_resourceManager->UpdateResourceToSend();
    m_sceneHolder->PreFrame(m_renderer.get());
    m_renderer->WaitUntilFrameFinished();
    if (!m_renderer->BeginFrame())
        return false;

    m_frameInProgress = true;
    return true;
}

void Engine::Update()
{
    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    m_deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
    lastTime = currentTime;

    if (!m_sceneHolder->GetCurrentScene())
        return;
    m_sceneHolder->GetCurrentScene()->UpdateEditorCamera(m_deltaTime);
    switch (m_runtimeMode)
    {
    case RuntimeMode::Edit:
        m_sceneHolder->UpdateEditor(m_deltaTime);
        break;
    case RuntimeMode::Play:
        m_sceneHolder->UpdateRuntime(m_deltaTime);
        break;
    case RuntimeMode::Pause:
        break;
    }
}

void Engine::Render()
{        
    m_sceneHolder->Render(m_renderer.get());
}

void Engine::EndFrame()
{
    m_renderer->EndFrame();
    m_frameInProgress = false;
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

void Engine::ApplyRuntimeMode(RuntimeMode mode)
{
    if (m_runtimeMode == mode)
        return;

    if (m_sceneHolder)
    {
        if (mode == RuntimeMode::Play && m_runtimeMode == RuntimeMode::Edit)
            m_sceneHolder->BeginPlay();
        else if (mode == RuntimeMode::Edit && m_runtimeMode != RuntimeMode::Edit)
            m_sceneHolder->EndPlay();
    }

    m_runtimeMode = mode;
}

void Engine::SetRuntimeMode(RuntimeMode mode)
{
    if (m_runtimeMode == mode && m_pendingRuntimeMode == RuntimeMode::Count)
        return;

    if (m_pendingRuntimeMode == mode)
        return;

    if (m_frameInProgress)
    {
        m_pendingRuntimeMode = mode;

        if (m_pendingRuntimeModeCallback == UUID_INVALID)
        {
            m_pendingRuntimeModeCallback = m_renderer->AddAfterRenderCallback([this]()
            {
                m_pendingRuntimeModeCallback = UUID_INVALID;

                if (m_pendingRuntimeMode == RuntimeMode::Count)
                    return;

                const RuntimeMode pendingMode = m_pendingRuntimeMode;
                m_pendingRuntimeMode = RuntimeMode::Count;

                m_renderer->WaitForGPU();

                ApplyRuntimeMode(pendingMode);
            });
        }
        return;
    }

    ApplyRuntimeMode(mode);
}

Engine* Engine::Get()
{
    return s_instance.get();
}
