#include <iostream>

#include "Core/Window.h"
#include "Debug/Log.h"

#include "Render/RHI/RHIRenderer.h"
#include "Render/Vulkan/VulkanRenderer.h"
#include "Resource/Mesh.h"

#include "Resource/ResourceManager.h"
#include "Resource/Model.h"
#include "Resource/Texture.h"

#include "Utils/Type.h"

int Run(int argc, char** argv, char** envp)
{
    (void)argc;
    (void)argv;
    (void)envp;

    WindowConfig config;
    config.title = "Window Test";
    config.size = Vec2i(1280, 720);
    config.attributes = static_cast<WindowAttributes>(VSync);
    std::unique_ptr<Window> window = Window::Create(WindowAPI::GLFW, RenderAPI::Vulkan, config);

    if (!window)
    {
        std::cerr << "Failed to create window" << '\n';
        return -1;
    }

    std::unique_ptr<RHIRenderer> renderer = RHIRenderer::Create(RenderAPI::Vulkan, window.get());
    if (!renderer)
    {
        std::cerr << "Failed to create renderer" << '\n';
        return -1;
    }

    ThreadPool::Initialize();

    std::unique_ptr<ResourceManager> resourceManager = std::make_unique<ResourceManager>();
    resourceManager->Initialize(renderer.get());
    resourceManager->LoadDefaultTexture("resources/textures/debug.jpeg");
    resourceManager->LoadDefaultShader("resources/shaders/unlit.shader");
    
    renderer->SetDefaultTexture(resourceManager->GetDefaultTexture());

    SafePtr<Model> cubeModel = resourceManager->Load<Model>("resources/models/Cube.obj");
    SafePtr cubeTexture = resourceManager->GetDefaultTexture();
    SafePtr cubeShader = resourceManager->GetDefaultShader();

    dynamic_cast<VulkanRenderer*>(renderer.get())->SetModel(cubeModel);
    dynamic_cast<VulkanRenderer*>(renderer.get())->SetTexture(cubeTexture);

    static auto startTime = std::chrono::high_resolution_clock::now(); 
    while (!window->ShouldClose())
    {
        auto currentTime = std::chrono::high_resolution_clock::now();
        window->PollEvents();

        resourceManager->UpdateResourceToSend();

        if (!renderer->IsInitialized() || !cubeShader->SentToGPU())
            continue;
        
        renderer->WaitUntilFrameFinished();
        // renderer->Update();
        {
            Vec2i windowSize = window->GetSize();
            static auto lastTime = std::chrono::high_resolution_clock::now();
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            static float time = 0.0f;
            time += deltaTime;

            // float fps = 1.0f / deltaTime;
            // PrintLog("FPS:   %f", fps);

            UniformBufferObject ubo;
            Vec3f camPos = Vec3f(2.0f, 2.0f, 2.0f);
            Vec3f camTarget = Vec3f(0.0f, 0.0f, 0.0f);
            Vec3f camUp = Vec3f(0.0f, 1.0f, 0.0f);

            float distanceInFront = 5.f;
            Vec3f forward = Vec3f::Normalize(camTarget - camPos);
            Vec3f cubePosition = camPos + forward * distanceInFront;

            float angle = time * 90.0f;
            ubo.Model = Mat4::CreateTransformMatrix(cubePosition, Vec3f(0.f, angle, 0.f), Vec3f(1.f, 1.f, 1.f));

            Quat camRotation = Quat::LookRotation(camTarget - camPos, camUp);

            Mat4 out = Mat4::CreateTransformMatrix(camPos, camRotation, Vec3f(1, 1, 1));
            ubo.View = Mat4::LookAtRH(camPos, camTarget, camUp);

            ubo.Projection = Mat4::CreateProjectionMatrix(
                45.f, (float)windowSize.x / (float)windowSize.y, 0.1f, 10.0f);
            ubo.Projection[1][1] *= -1; // GLM -> Vulkan Y flip

            cubeShader->SendValue(&ubo, sizeof(ubo), renderer.get());
        }
        if (!renderer->BeginFrame())
            continue;
        
        renderer->ClearColor();

        renderer->BindShader(cubeShader.get().get());

        // Draw the model if available
        if (cubeModel && cubeModel->IsLoaded() && cubeModel->SentToGPU())
        {
            auto meshes = cubeModel->GetMeshes();

            for (auto* mesh : meshes)
            {
                if (!mesh || !mesh->GetVertexBuffer() || !mesh->GetIndexBuffer())
                    continue;

                renderer->DrawVertex(mesh->GetVertexBuffer(), mesh->GetIndexBuffer());
            }
        }
        
        renderer->EndFrame();
        
    }

    // Wait for GPU to finish rendering before cleaning
    renderer->WaitForGPU();
    
    ThreadPool::WaitUntilAllTasksFinished();

    resourceManager->Clear();
    renderer->Cleanup();
    
    ThreadPool::Terminate();

    window->Terminate();

    return 0;
}


#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if defined(_WIN32) && defined(NDEBUG)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nCmdShow)
#else
int main(int argc, char** argv, char** envp)
#endif
{
#ifdef _WIN32
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // TODO: Remove Comments To Break on leaks
    // |
    // V
    // _CrtSetBreakAlloc(173);
#ifdef NDEBUG
    int argc = __argc;
    char** argv = __argv;
    char** envp = nullptr;
#endif
#endif
    return Run(argc, argv, envp);
}