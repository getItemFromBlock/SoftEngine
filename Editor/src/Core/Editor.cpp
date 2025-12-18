#include "Editor.h"

#include "Component/MeshComponent.h"
#include "Component/TestComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Engine.h"

#include "Resource/Mesh.h"
#include "Resource/Model.h"
#include "Scene/GameObject.h"
#include "Utils/Color.h"

Editor::Editor()
{
}

void Editor::Initialize()
{
    WindowConfig config;
    config.title = "Window Test";
    config.size = Vec2i(1280, 720);
    config.attributes = static_cast<WindowAttributes>(VSync);
    m_window = Window::Create(WindowAPI::GLFW, RenderAPI::Vulkan, config);
    
    EngineDesc desc = {
        .window = m_window.get(),
    };
    
    m_engine = Engine::Create();
    m_engine->Initialize(desc);
    
    m_imguiHandler = std::make_unique<ImGuiHandler>();
    m_imguiHandler->Initialize(m_window.get(), m_engine->GetRenderer());
    
    m_windowManager = std::make_unique<EditorWindowManager>();
    m_windowManager->Initialize(m_engine);
    
    auto resourceManager = m_engine->GetResourceManager();
    auto currentScene = m_engine->GetSceneHolder()->GetCurrentScene();
    SafePtr cubeModel = resourceManager->Load<Model>(RESOURCE_PATH"/models/Cube.obj");
    cubeModel = resourceManager->Load<Model>(RESOURCE_PATH"/models/Suzanne.obj");
    cubeModel->EOnLoaded.Bind([cubeModel, this, resourceManager, currentScene]()
    {
        SafePtr cubeMesh = resourceManager->GetResource<Mesh>(cubeModel->GetMeshes()[0]->GetPath());
    
        size_t count = std::pow(3, 3);
        int sqrtCount = std::pow(count, 1 / 3.f);
            
        auto mat = resourceManager->GetDefaultMaterial();
        mat->SetAttribute("albedoSampler", resourceManager->GetDefaultTexture());
        mat->SetAttribute("color", (Vec4f)Color::Red);
        for (int i = 0; i < count; i++)
        {
            float hue = i * 360 / count;
            
            SafePtr<GameObject> object = currentScene->CreateGameObject();
            
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
            
            object->AddComponent<TestComponent>();
        }
    });
}

void Editor::Run()
{    
    while (!m_window->ShouldClose())
    {
        m_window->PollEvents();
        
        if (!m_engine->BeginFrame())
            continue;
        
        m_engine->Update();
        
        m_engine->Render();
        
        m_imguiHandler->BeginFrame();
        OnRender();
        m_imguiHandler->EndFrame();
        
        m_engine->EndFrame();
    }
}

void Editor::OnRender()
{
    m_windowManager->Render();
}

void Editor::Cleanup()
{
    m_engine->WaitBeforeClean();
    m_imguiHandler->Cleanup();
    m_engine->Cleanup();
}
