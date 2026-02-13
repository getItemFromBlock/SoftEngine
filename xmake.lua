add_rules("plugin.vsxmake.autoupdate")
add_rules("plugin.compile_commands.autoupdate", {outputdir = ".vscode"})
add_rules("mode.debug", "mode.release")

if is_plat("windows") then
    set_runtimes(is_mode("debug") and "MDd" or "MD")
    add_cxxflags("/wd4100")
elseif is_plat("macosx") then
    add_cxxflags("-Wno-enum-enum-conversion", {force = true})
    add_cxxflags("-Wno-error=deprecated-declarations", {force = true})
end

add_repositories("galaxy-repo https://github.com/GalaxyEngine/xmake-repo")

add_requires("vulkansdk")
add_requires("shaderc")
add_requires("spirv-reflect")
add_requires("glfw")

-- ImGui configuration
local imgui_configs = {}
imgui_configs.glfw = true
imgui_configs.vulkan = true

add_requires("imgui v1.92.5-docking", {configs = imgui_configs, debug = true})
add_requires("stb")
add_requires("galaxymath", "cpp_serializer")
add_requires("thread-pool")

-- Define macros
add_defines("NOMINMAX", "IMGUI_IMPLEMENTATION")

add_packages("glfw")

add_packages("vulkansdk")
add_packages("spirv-reflect")
add_packages("shaderc")
if is_plat("windows", "mingw") and is_mode("debug") then
    add_links("shaderc_combinedd")
else
    add_links("shaderc_combined")
end

if is_plat("windows") then
    add_links("vulkan-1")
elseif is_plat("linux") or is_plat("macosx") then
    add_links("vulkan")
end

-- Platform-specific settings
if is_plat("windows") then
    add_syslinks("user32", "gdi32", "shell32")
elseif is_plat("macosx") then
    add_frameworks("Cocoa", "IOKit", "CoreVideo")
end

set_languages("c++latest")
set_rundir("$(projectdir)")

add_defines("MULTI_THREAD")

target("Engine")
    set_kind("static")
    add_defines("ENGINE_EXPORTS")
    
    add_files("Engine/src/**.cpp")
    add_headerfiles("Engine/src/**.h", "Engine/src/**.hpp")
    add_includedirs("Engine/src")

    -- Always add base packages
    add_packages("galaxymath", "stb", "thread-pool", "cpp_serializer")
    
    add_cxxflags("-Wall", "-Wextra")
target_end()

target("Editor")
    set_default(true)
    set_kind("binary")
    
    add_deps("Engine")
    
    add_defines("IMGUI_DEFINE_MATH_OPERATORS")
    
    add_files("Editor/src/**.cpp")
    add_headerfiles("Editor/src/**.h", "Editor/src/**.hpp")
    add_includedirs("Editor/src",  "Engine/src")
    
    add_packages("imgui", "galaxymath", "stb", "thread-pool")
target_end()

includes("Tests/*.lua")