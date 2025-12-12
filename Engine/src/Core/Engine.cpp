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

void Engine::Create()
{
    if (s_instance)
        return;
    s_instance = std::make_unique<Engine>();
}

bool Engine::Initialize()
{
    WindowConfig config;
    config.title = "Window Test";
    config.size = Vec2i(1280, 720);
    config.attributes = static_cast<WindowAttributes>(VSync);
    m_window = Window::Create(WindowAPI::GLFW, RenderAPI::Vulkan, config);

    if (!m_window)
    {
        PrintError("Failed to create window");
        return false;
    }

    m_renderer = RHIRenderer::Create(RenderAPI::Vulkan, m_window.get());
    if (!m_renderer)
    {
        PrintError("Failed to create renderer");
        return false;
    }

    ThreadPool::Initialize();

    m_resourceManager = std::make_unique<ResourceManager>();
    m_resourceManager->Initialize(m_renderer.get());
    m_resourceManager->LoadDefaultTexture(RESOURCE_PATH"/textures/debug.jpeg");
    m_resourceManager->LoadDefaultShader(RESOURCE_PATH"/shaders/unlit.shader");
    m_resourceManager->LoadDefaultMaterial(RESOURCE_PATH"/shaders/unlit.material");
    
    m_componentRegister = std::make_unique<ComponentRegister>();
    m_componentRegister->RegisterComponent<TransformComponent>();
    m_componentRegister->RegisterComponent<MeshComponent>();
    
    return true;
}

void Engine::Run()
{
    m_scene = std::make_unique<Scene>();
    
    SafePtr cubeModel = m_resourceManager->Load<Model>(RESOURCE_PATH"/models/Suzanne.obj");
    cubeModel->OnLoaded.Bind([&]()
    {
        SafePtr cubeMesh = m_resourceManager->GetResource<Mesh>(cubeModel->GetMeshes()[0]->GetPath());
    
        size_t count = std::pow(3, 3);
        float sqrtCount = std::pow(count, 1 / 3.f);
            
        for (int i = 0; i < count; i++)
        {
            auto mat = m_resourceManager->CreateMaterial("Material_" + std::to_string(i));

            float hue = i * 360 / count;
            
            mat->SetAttribute("color", static_cast<Vec4f>(Color::FromHSV(hue, 1.f, 1.f)));
            
            SafePtr<GameObject> object = m_scene->CreateGameObject();
            
            SafePtr<TransformComponent> transform = object->GetComponent<TransformComponent>();
            
            int N = sqrtCount;

            int ix = (i % N);
            int iy = (i / N) % N;
            int iz = (i / (N * N));

            float cx = (N - 1) * 0.5f;

            float x = (ix - cx) * 2.5f;
            float y = (iy - cx) * 2.5f;
            float z = (iz - cx) * 2.5f;
            
            transform->SetLocalPosition(Vec3f(x, y, z));
            
            SafePtr<MeshComponent> meshComp = object->AddComponent<MeshComponent>();
            meshComp->SetMesh(cubeMesh);
            meshComp->AddMaterial(mat);
            
            {
                object->AddComponent<TestComponent>();
            }
        }
        m_ready = true;
    });
    SafePtr cubeShader = m_resourceManager->GetDefaultShader();

    static auto startTime = std::chrono::high_resolution_clock::now(); 
    while (!m_window->ShouldClose())
    {
        static auto lastTime = std::chrono::high_resolution_clock::now();
        static float time = 0.0f;
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        time += deltaTime;
        lastTime = currentTime;
                
        m_window->PollEvents();

        m_resourceManager->UpdateResourceToSend();

        if (!m_renderer->IsInitialized() || !cubeShader || !cubeShader->SentToGPU() || !m_ready)
            continue;
        
        m_renderer->WaitUntilFrameFinished();

        m_scene->OnUpdate(deltaTime);
        if (!m_renderer->BeginFrame())
            continue;
        
        m_renderer->ClearColor();

        m_scene->OnRender(m_renderer.get());
        
        m_renderer->EndFrame();
    }
}

void Engine::Cleanup() const
{
    // Wait for GPU to finish rendering before cleaning
    m_renderer->WaitForGPU();
    
    ThreadPool::WaitUntilAllTasksFinished();

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