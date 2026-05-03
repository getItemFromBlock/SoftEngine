#include "Editor.h"
#include "Core/Engine.h"

#include "Component/MeshComponent.h"
#include "Component/GPUSoftBodyComponent.h"
#include "Component/ProceduralSoftBodyComponent.h"
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
    
    auto sponza = resourceManager->Load<Model>(RESOURCE_PATH"models/Cube.obj");
    sponza->EOnLoaded.Bind([this, currentScene, sponza]()
    {
        auto go = Model::CreateGameObject(sponza.getPtr(), currentScene);
        go->GetTransform()->SetLocalPosition({0, 0, -5});
    });
    // model = resourceManager->Load<Model>(RESOURCE_PATH"/models/Sponza/sponza.obj");
    model = resourceManager->Load<Model>(RESOURCE_PATH"models/Plane.obj");

    model->EOnLoaded.Bind([model, this, currentScene, resourceManager]()
    {
        auto sphereMaterial = resourceManager->CreateMaterial("SphereMat", resourceManager->GetDefaultShader());

        auto albedo = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/toy_box_diffuse.png");
        auto normal = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/toy_box_normal.png");
        auto metallic = resourceManager->Load<Texture>(RESOURCE_PATH"textures/blank.png");
        auto roughness = resourceManager->Load<Texture>(RESOURCE_PATH"textures/blank.png");
        auto ao = resourceManager->Load<Texture>(RESOURCE_PATH"textures/blank.png");
        auto height = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/toy_box_disp.png");

        auto unorm = TextureParam{.format = TextureFormat::UNORM};
        normal->SetTextureParameters(unorm);
        roughness->SetTextureParameters(unorm);
        metallic->SetTextureParameters(unorm);
        ao->SetTextureParameters(unorm);
        height->SetTextureParameters(unorm);

        sphereMaterial->SetAttribute("material.color", Vec4f::One());
        sphereMaterial->SetAttribute("albedoSampler", albedo);
        sphereMaterial->SetAttribute("normalSampler", normal);
        sphereMaterial->SetAttribute("roughnessSampler", roughness);
        sphereMaterial->SetAttribute("metalnessSampler", metallic);
        sphereMaterial->SetAttribute("aoSampler", ao);
        sphereMaterial->SetAttribute("heightSampler", height);
        sphereMaterial->SetAttribute("material.roughnessFactor", 1.f);
        sphereMaterial->SetAttribute("material.metalnessFactor", 1.f);
        sphereMaterial->SetAttribute("material.aoFactor", 1.f);
        sphereMaterial->SetAttribute("material.heightScale", 0.1f);

        Vec3f position = {-4, 0, 0};
        auto go = Model::CreateGameObject(model.getPtr(), currentScene);
        go->GetTransform()->SetLocalPosition(position);
        go->GetComponent<MeshComponent>()->SetMaterial(0, sphereMaterial);
        
        auto light = currentScene->CreateGameObject();
        light->SetName("Light");
        light->AddComponent<LightComponent>()->SetIntensity(1.f);
        light->GetTransform()->SetLocalPosition(Vec3f(0,0,-3.0f));
        //light->AddComponent<TestComponent>()->AttachToCamera(true);
        
    });

    model->EOnLoaded.Bind([model, this, currentScene]()
    {
        loadedA = true;
    });

    model = resourceManager->Load<Model>(RESOURCE_PATH"models/Barrel.obj");
    model->EOnLoaded.Bind([model, this, currentScene]()
    {
        loadedB = true;
    });
}


void Editor::Run()
{
    bool renderImGui = true;

    auto* camera = m_engine->GetSceneHolder()->GetCurrentScene()->GetEditorCamera();
    auto* renderer = m_engine->GetRenderer();

    camera->SetRenderTargetSize(renderer->GetSwapChain()->GetExtent().width,
                                renderer->GetSwapChain()->GetExtent().height);

    m_window->EResizeEvent.Bind([&](const Vec2i& size)
    {
        if (!renderImGui)
        {
            camera->SetRenderTargetSize(size.x, size.y);
        }
    });

    while (!m_window->ShouldClose())
    {
        if (!initialised && loadedA && loadedB)
        {
            initialised = true;
            auto currentScene = m_engine->GetSceneHolder()->GetCurrentScene();
            auto go0 = Model::CreateGameObject(
                m_engine->GetResourceManager()->Load<Model>(RESOURCE_PATH"models/Sphere.obj").getPtr(), currentScene);
            go0->GetTransform()->SetLocalPosition(Vec3f(4, 0, 0));

            auto go1 = currentScene->CreateGameObject();
            go1->SetName("SoftBody");
            auto soft1 = go1->AddComponent<GPUSoftBodyComponent>();
            soft1->CreateFromMesh(
                m_engine->GetResourceManager()->Load<Mesh>(RESOURCE_PATH"/models/Barrel.obj/Cylinder.mesh"));

            auto go2 = currentScene->CreateGameObject();
            go2->SetName("ProceduralSoftBody");
            auto soft2 = go2->AddComponent<ProceduralSoftBodyComponent>();
        }

        m_window->PollEvents();

        if (m_window->GetInput().IsKeyPressed(Key::F1))
        {
            renderImGui = !renderImGui;
            if (!renderImGui)
            {
                auto windowSize = m_window->GetSize();
                camera->SetRenderTargetSize(windowSize.x, windowSize.y);
            }
        }

        if (!m_engine->BeginFrame())
            continue;

        m_engine->Update();
        m_engine->Render();

        if (renderImGui)
        {
            m_imguiHandler->BeginFrame();
            OnRender();
            m_imguiHandler->EndFrame();
        }
        else
        {
            camera->BlitToSwapchain(renderer);
        }

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
