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
    
    auto lightObject = currentScene->CreateGameObject();
    lightObject->AddComponent<LightComponent>();
    lightObject->AddComponent<TestComponent>()->AttachToCamera(true);
    
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
            sphereMaterial->SetAttribute("material.aoFactor", 1.f);

            auto go = Model::CreateGameObject(model.getPtr(), currentScene, parent.getPtr());
            go->GetTransform()->SetLocalPosition(Vec3f(0.0f, 0.0f, 0.0f));
            go->GetComponent<MeshComponent>()->SetMaterial(0, sphereMaterial);
        }
    });

    auto cube = resourceManager->Load<Model>(RESOURCE_PATH"models/Cube.obj");
    cube->EOnLoaded.Bind([cube, this, currentScene]()
    {
        auto parent = currentScene->CreateGameObject();
        parent->SetName("Lights");
    
        auto resourceManager = m_engine->GetResourceManager();
        auto unlitForwardShader = resourceManager->Load<Shader>(RESOURCE_PATH"/shaders/Unlit/unlit.shader");
    
        float spacing2 = 2.66f;
        int cubeCount = 27;
        int dim = std::ceil(std::cbrt(cubeCount));

        int x = 0, y = 0, z = 0;
        
        for (size_t i = 0; i < cubeCount; i++)
        {
            auto color = resourceManager->CreateMaterial("Material_" + std::to_string(i), unlitForwardShader);
            Vec4f attribute = static_cast<Vec4f>(Color::FromHSV((float(i) / float(cubeCount)) * 360.f, 1.f, 1.f));
            attribute.w = 1.f;
            color->SetAttribute("material.color", attribute);

            SafePtr<GameObject> light = Model::CreateGameObject(cube.getPtr(), currentScene, parent.getPtr());
            SafePtr<MeshComponent> meshComponent = light->GetComponent<MeshComponent>();
            meshComponent->SetMaterial(0, color);

            light->GetTransform()->SetLocalScale(Vec3f(0.25f));

            x =  i % dim;
            y = (i / dim) % dim;
            z =  i / (dim * dim);

            float half = (dim - 1) * 0.5f;

            light->GetTransform()->SetLocalPosition(
                Vec3f(
                    (x - half) * spacing2,
                    (y - half) * spacing2,
                    (z - half) * spacing2
                )
            );

            light->SetName("Light " + std::to_string(i));

            auto lightComponent = light->AddComponent<LightComponent>();
            lightComponent->SetColor(attribute);
            lightComponent->SetIntensity(1.f);

            auto testComponent = light->AddComponent<TestComponent>();
            testComponent->SetOffset(i);
        }
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
