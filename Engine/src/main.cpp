#include <iostream>
#include "Core/Engine.h"

int Run(int argc, char** argv, char** envp)
{
    (void)argc; (void)argv; (void)envp;

    Engine::Create();
    Engine* engine = Engine::Get();
    engine->Initialize();
    engine->Run();
    engine->Cleanup();
    return 0;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef _MSC_VER
#include <crtdbg.h>
#endif


#if defined(_WIN32) && defined(_MSC_VER) && defined(NDEBUG)
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int cmdShow)
{
    (void)hInst; (void)hPrev; (void)cmdLine; (void)cmdShow;

    int argc = __argc;
    char** argv = __argv;
    char** envp = nullptr;

    return Run(argc, argv, envp);
}

#else
int main(int argc, char** argv, char** envp)
{
#if defined(_MSC_VER) && !defined(NDEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    // _CrtSetBreakAlloc(2368);
#endif

    return Run(argc, argv, envp);
}
#endif
