#include <iostream>

#include "Core/Editor.h"

int Run(int argc, char** argv, char** envp)
{
    (void)argc; (void)argv; (void)envp;

    Editor* editor = Editor::Create();
    editor->Initialize();
    editor->Run();
    editor->Cleanup();
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
    // _CrtSetBreakAlloc(3248);
#endif

    //TODO: ClassDescriptor added to class or at least to a manager, that do for all loaded resources (for memory management), 
    return Run(argc, argv, envp);
}
#endif
