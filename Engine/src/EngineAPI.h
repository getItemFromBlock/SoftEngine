// #if defined(_WIN32)
// 	#if defined(ENGINE_EXPORTS)
//         #define ENGINE_API __declspec(dllexport)
//     #else
//         #define ENGINE_API __declspec(dllimport)
//     #endif // ENGINE_EXPORTS
// #elifdef __linux__ || __APPLE__
//     #define ENGINE_API __attribute__((visibility("default")))
// #endif

#define ENGINE_API 