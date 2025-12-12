#include "Editor.h"

#include "Core/Engine.h"

Editor::Editor()
{
    Engine::Create();
    m_engine = Engine::Get();
}

void Editor::Initialize()
{
    m_engine->Initialize();
    m_imguiHandler = std::make_unique<ImGuiHandler>();
    m_imguiHandler->Initialize(m_engine->GetWindow(), m_engine->GetRenderer());
    
    m_engine->OnRender += [&]()
    {
        m_imguiHandler->BeginFrame();
        OnRender();
    };
    
    m_engine->OnEndFrame += [&]()
    {
        m_imguiHandler->EndFrame();
    };
    
    m_engine->OnCleanup += [&]()
    {
        m_imguiHandler->Cleanup();
    };
}

void Editor::Run()
{
    m_engine->Run();
}

void Editor::OnRender()
{
    ImGui::ShowDemoWindow();
}

void Editor::Cleanup()
{
    m_engine->Cleanup();
}
