#include "Editor.h"
#include "Core/Engine.h"

#include "Component/MeshComponent.h"
#include "Component/GPUSoftBodyComponent.h"
#include "Component/TestComponent.h"
#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"

#include "Resource/ComputeShader.h"
#include "Resource/Mesh.h"
#include "Resource/Model.h"
#include "Resource/CubeMap.h"
#include "Resource/PostProcessShader.h"

#include "Scene/GameObject.h"

#include "Utils/Color.h"

Editor::Editor()
{
}

Editor* Editor::Create()
{
    if (!s_instance)
    {
        s_instance = std::make_unique<Editor>();
    }
    return s_instance.get();
}

void Editor::Initialize()
{
    WindowConfig config;
    config.title = "Editor";
    config.size = Vec2i(1280, 720);
    config.attributes = static_cast<WindowAttributes>(VSync);
    m_window = Window::Create(WindowAPI::GLFW, config);
    
    EngineDesc desc = {
        .window = m_window.get(),
    };
    
    m_engine = Engine::Create();
    m_engine->Initialize(desc);
    
    m_imguiHandler = std::make_unique<ImGuiHandler>();
    m_imguiHandler->Initialize(m_window.get(), m_engine->GetRenderer());
    
    m_windowManager = std::make_unique<EditorWindowManager>();
    m_windowManager->Initialize(m_engine, m_imguiHandler.get());
    
    auto resourceManager = m_engine->GetResourceManager();
    auto currentScene = m_engine->GetSceneHolder()->GetCurrentScene();
    
    auto model = resourceManager->Load<Model>(RESOURCE_PATH"/models/Cube.obj");
    resourceManager->Load<Model>(RESOURCE_PATH"/models/Suzanne.obj");
    resourceManager->Load<Model>(RESOURCE_PATH"/models/Plane.obj");
    resourceManager->Load<CubeMap>(RESOURCE_PATH"/envMap/wooden_studio_09_4k.hdr");
    resourceManager->Load<PostProcessShader>(RESOURCE_PATH"/shaders/PostProcess/inverted.pshader");
    // model = resourceManager->Load<Model>(RESOURCE_PATH"/models/Sponza/sponza.obj");
    model = resourceManager->Load<Model>(RESOURCE_PATH"models/Sphere.obj");
    
    model->EOnLoaded.Bind([model, this, currentScene]()
    {
        auto go = Model::CreateGameObject(model.getPtr(), currentScene);
        go->GetTransform()->SetLocalPosition(Vec3f(4, 0, 0));
    });
    
    model = resourceManager->Load<Model>(RESOURCE_PATH"models/Cylinder.obj");
    model->EOnLoaded.Bind([model, this, currentScene]()
    {
        auto go = currentScene->CreateGameObject();
        go->AddComponent<GPUSoftBodyComponent>();
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
        //std::this_thread::sleep_for(std::chrono::milliseconds(50));
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
