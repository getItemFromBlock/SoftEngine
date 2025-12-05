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
#include "Component/TransformComponent.h"
#include "Scene/GameObject.h"

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
    
    m_componentRegister = std::make_unique<ComponentRegister>();
    m_componentRegister->RegisterComponent<TransformComponent>();
    m_componentRegister->RegisterComponent<MeshComponent>();
    
    return true;
}

void Engine::Run()
{
    Scene scene;
    SafePtr cubeModel = m_resourceManager->Load<Model>(RESOURCE_PATH"/models/Cube.obj");
    cubeModel->OnLoaded.Bind([&]()
    {
        SafePtr cubeMesh = m_resourceManager->GetResource<Mesh>(cubeModel->GetMeshes()[0]->GetPath());
    
        SafePtr<GameObject> object = scene.CreateGameObject();
        SafePtr<MeshComponent> meshComp = object->AddComponent<MeshComponent>();
        meshComp->SetMesh(cubeMesh);
    });
    SafePtr cubeShader = m_resourceManager->GetDefaultShader();

    bool process = false;
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

        if (!m_renderer->IsInitialized() || !cubeShader || !cubeShader->SentToGPU())
            continue;
        
        m_renderer->WaitUntilFrameFinished();
        
        {
            Vec2i windowSize = m_window->GetSize();

            UniformBufferObject ubo;
            Vec3f camPos = Vec3f(2.0f, 2.0f, 2.0f);
            Vec3f camTarget = Vec3f(0.0f, 0.0f, 0.0f);
            Vec3f camUp = Vec3f(0.0f, 1.0f, 0.0f);

            float distanceInFront = 5.f;
            Vec3f forward = Vec3f::Normalize(camTarget - camPos);
            Vec3f cubePosition = camPos + forward * distanceInFront;

            float angle = time * 90.0f;
            ubo.Model = Mat4::CreateTransformMatrix(cubePosition, Vec3f(0.f, angle, 0.f), Vec3f(1.f, 1.f, 1.f));

            ubo.View = Mat4::LookAtRH(camPos, camTarget, camUp);

            ubo.Projection = Mat4::CreateProjectionMatrix(
                45.f, (float)windowSize.x / (float)windowSize.y, 0.1f, 10.0f);
            ubo.Projection[1][1] *= -1; // GLM -> Vulkan Y flip

            cubeShader->SendValue(&ubo, sizeof(ubo), m_renderer.get());
        }
        scene.OnUpdate(deltaTime);
        if (!m_renderer->BeginFrame())
            continue;
        
        m_renderer->ClearColor();

        m_renderer->BindShader(cubeShader.get().get());

        // Draw the model if available
        if (cubeModel && cubeModel->IsLoaded() && cubeModel->SentToGPU())
        {
            auto meshes = cubeModel->GetMeshes();

            for (auto* mesh : meshes)
            {
                if (!mesh || !mesh->GetVertexBuffer() || !mesh->GetIndexBuffer())
                    continue;

                m_renderer->DrawVertex(mesh->GetVertexBuffer(), mesh->GetIndexBuffer());
            }
        }
        
        m_renderer->EndFrame();
        
    }
}

void Engine::Cleanup() const
{
    // Wait for GPU to finish rendering before cleaning
    m_renderer->WaitForGPU();
    
    ThreadPool::WaitUntilAllTasksFinished();

    m_resourceManager->Clear();
    m_renderer->Cleanup();
    
    ThreadPool::Terminate();

    m_window->Terminate();
}
