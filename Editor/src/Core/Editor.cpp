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

    // ===== Debug ===== //
    auto model = resourceManager->Load<Model>(RESOURCE_PATH"/models/Cube.obj");
    resourceManager->Load<Model>(RESOURCE_PATH"/models/Suzanne.obj");
    resourceManager->Load<Model>(RESOURCE_PATH"/models/Plane.obj");
    resourceManager->Load<CubeMap>(RESOURCE_PATH"/envMap/wooden_studio_09_4k.hdr");
    resourceManager->Load<PostProcessShader>(RESOURCE_PATH"/shaders/PostProcess/inverted.pshader");
    model = resourceManager->Load<Model>(RESOURCE_PATH"models/Sphere.obj");


    model->EOnLoaded.Bind([model, this, currentScene, resourceManager]()
    {
        auto parent = currentScene->CreateGameObject();
        parent->SetName("Spheres");
        {
            auto sphereMaterial = resourceManager->CreateMaterial("SphereMat", resourceManager->GetDefaultShader());
            
            auto albedo = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/albedo.png");
            auto normal = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/normal.png");
            auto metallic = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/metallic.png");
            auto roughness = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/roughness.png");
            auto ao = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/ao.png");
        
            /*
            auto albedo = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/PavingStones150_4K-PNG_Color.png");
            auto normal = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/PavingStones150_4K-PNG_NormalGL.png");
            auto roughness = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/PavingStones150_4K-PNG_Roughness.png");
            auto metallic = resourceManager->Load<Texture>(RESOURCE_PATH"textures/black.png");
            */
            
            auto unorm = TextureParam{.format = TextureFormat::UNORM};
            auto r8_unorm = TextureParam{.format = TextureFormat::R8_UNORM};
            normal->SetTextureParameters(unorm);
            roughness->SetTextureParameters(unorm);
            metallic->SetTextureParameters(unorm);
            ao->SetTextureParameters(unorm);
            
            sphereMaterial->SetAttribute("material.color", Vec4f::One());
            sphereMaterial->SetAttribute("albedoSampler", albedo);
            sphereMaterial->SetAttribute("normalSampler", normal);
            sphereMaterial->SetAttribute("roughnessSampler", roughness);
            sphereMaterial->SetAttribute("metalnessSampler", metallic);
            sphereMaterial->SetAttribute("aoSampler", ao);
            sphereMaterial->SetAttribute("material.roughnessFactor", 1.f);
            sphereMaterial->SetAttribute("material.metalnessFactor", 1.f);

            auto go = Model::CreateGameObject(model.getPtr(), currentScene, parent.getPtr());
            go->GetTransform()->SetLocalPosition(Vec3f(0.0f, 0.0f, 0.0f));
            go->GetComponent<MeshComponent>()->SetMaterial(0, sphereMaterial);
        }
        
        {
            auto pbrForward = resourceManager->Load<Shader>(RESOURCE_PATH"shaders/PBR/pbr.shader");
            auto sphereMaterialForward = resourceManager->CreateMaterial("SphereMatForward", pbrForward);
            sphereMaterialForward->SetAttribute("material.color", Vec4f::One());

            auto go = Model::CreateGameObject(model.getPtr(), currentScene, parent.getPtr());
            go->GetTransform()->SetLocalPosition(Vec3f(2.0f, 0.0f, 0.0f));
            go->GetComponent<MeshComponent>()->SetMaterial(0, sphereMaterialForward);
        }
        
        {
            auto normalShader = resourceManager->Load<Shader>(RESOURCE_PATH"shaders/Normal/normal.shader");
            auto normalMaterial = resourceManager->CreateMaterial("Debug Sphere", normalShader);
            
            
            auto go = Model::CreateGameObject(model.getPtr(), currentScene, parent.getPtr());
            go->GetTransform()->SetLocalPosition(Vec3f(-2.0f, 0.0f, 0.0f));
            go->GetComponent<MeshComponent>()->SetMaterial(0, normalMaterial);
        }
    });

    auto cube = resourceManager->Load<Model>(RESOURCE_PATH"models/Cube.obj");
    cube->EOnLoaded.Bind([cube, this, currentScene]()
    {
        auto parent = currentScene->CreateGameObject();
        parent->SetName("Lights");

        auto resourceManager = m_engine->GetResourceManager();
        auto unlitForwardShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/Unlit/unlit.shader");
        auto red = resourceManager->CreateMaterial("Red", unlitForwardShader);
        red->SetAttribute("material.color", Vec4f(1, 0, 0, 1));

        auto green = resourceManager->CreateMaterial("Green", unlitForwardShader);
        green->SetAttribute("material.color", Vec4f(0, 1, 0, 1));

        auto blue = resourceManager->CreateMaterial("Blue", unlitForwardShader);
        blue->SetAttribute("material.color", Vec4f(0, 0, 1, 1));

        float spacing2 = 2.66f;
        for (size_t i = 0; i < 3; i++)
        {
            SafePtr<GameObject> light = Model::CreateGameObject(cube.getPtr(), currentScene, parent.getPtr());
            SafePtr<MeshComponent> meshComponent = light->GetComponent<MeshComponent>();
            meshComponent->SetMaterial(0, i % 3 == 0 ? red : i % 3 == 1 ? green : blue);

            light->GetTransform()->SetLocalScale(Vec3f(0.25f));
            float x = (i % 3) - 1.0f;
            float z = (i / 3) - 1.0f;
            light->GetTransform()->SetLocalPosition(Vec3f(x * spacing2, 0.0f, z * spacing2));
            light->SetName("Light " + std::to_string(i));

            auto lightComponent = light->AddComponent<LightComponent>();
            Vec3f color = Vec3f::Zero();
            color[(i % 3)] = 1.0f;
            lightComponent->SetColor(color);
            lightComponent->SetIntensity(10.f);

            auto testComponent = light->AddComponent<TestComponent>();
            testComponent->SetOffset(i);
        }
    });

    auto plane = resourceManager->Load<Model>(RESOURCE_PATH"models/Plane.obj");
    plane->EOnLoaded.Bind([plane, this, currentScene, resourceManager]()
    {
        // auto sphereMaterial = resourceManager->CreateMaterial("Ground Material", resourceManager->GetDefaultShader());
        // auto albedo = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/PavingStones150_4K-PNG_Color.png");
        // auto normal = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/PavingStones150_4K-PNG_NormalGL.png");
        // auto roughness = resourceManager->Load<Texture>(RESOURCE_PATH"textures/pbr/PavingStones150_4K-PNG_Roughness.png");
        // sphereMaterial->SetAttribute("material.color", Vec4f::One());
        // sphereMaterial->SetAttribute("albedoSampler", albedo);
        // sphereMaterial->SetAttribute("normalSampler", normal);
        // sphereMaterial->SetAttribute("roughnessSampler", roughness);
        // sphereMaterial->SetAttribute("material.roughnessFactor", 0.5f);
        // sphereMaterial->SetAttribute("material.metalnessFactor", 0.0f);
        auto unlitForwardShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/Unlit/unlit.shader");
        auto groundMaterial = resourceManager->CreateMaterial("Ground", unlitForwardShader);
        groundMaterial->SetAttribute("material.color", Vec4f(0.5f, 0.5f, 0.5f, 1.0f));

        auto go = Model::CreateGameObject(plane.getPtr(), currentScene);
        auto transformComponent = go->GetTransform();
        transformComponent->SetLocalPosition(Vec3f(0.0f, -2.0f, 0.0f));
        transformComponent->SetLocalRotation(Quat::FromEuler(Vec3f(90.0f, 0.0f, 0.0f)));
        transformComponent->SetLocalScale(Vec3f(10.0f, 10.0f, 1.0f));

        auto meshComponent = go->GetComponent<MeshComponent>();
        meshComponent->SetMaterial(0, groundMaterial);
    });
    // ===== Debug ===== //
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
