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
