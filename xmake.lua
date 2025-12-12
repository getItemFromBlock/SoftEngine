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

-- Window API Options
option("glfw")
    set_default(true)
    set_showmenu(true)
    set_description("Enable GLFW window backend")
option_end()

option("sdl")
    set_default(true)
    set_showmenu(true)
    set_description("Enable SDL2 window backend")
option_end()

-- Render API Options
option("opengl")
    set_default(true)
    set_showmenu(true)
    set_description("Enable OpenGL rendering backend")
option_end()

option("vulkan")
    set_default(true)
    set_showmenu(true)
    set_description("Enable Vulkan rendering backend")
option_end()

option("directx")
    set_default(false)
    set_showmenu(true)
    set_description("Enable DirectX rendering backend (Windows only)")
option_end()

add_repositories("galaxy-repo https://github.com/GalaxyEngine/xmake-repo")

-- Add required packages based on options
if has_config("vulkan") then
    add_requires("vulkansdk")
    add_requires("shaderc")
    add_requires("spirv-reflect")
end

if has_config("glfw") then
    add_requires("glfw")
end

if has_config("sdl") then
    add_requires("libsdl2")
end

-- ImGui configuration
local imgui_configs = {}
if has_config("glfw") then
    imgui_configs.glfw = true
end
if has_config("vulkan") then
    imgui_configs.vulkan = true
end
if has_config("opengl") then
    imgui_configs.opengl = true
end

add_requires("imgui v1.92.0-docking", {configs = imgui_configs})
add_requires("stb")
add_requires("galaxymath")
add_requires("thread-pool")

-- Define macros
add_defines("NOMINMAX")

if has_config("glfw") then
    add_packages("glfw")
    add_defines("WINDOW_API_GLFW")
end

if has_config("sdl") then
    add_packages("libsdl2")
    add_defines("WINDOW_API_SDL")
end

-- Add Render API packages and defines
if has_config("opengl") then
    add_defines("RENDER_API_OPENGL")
    if is_plat("windows") then
        add_links("opengl32")
    elseif is_plat("linux") then
        add_links("GL")
    elseif is_plat("macosx") then
        add_frameworks("OpenGL")
    end
end

if has_config("vulkan") then
    add_packages("vulkansdk")
    add_packages("spirv-reflect")
    add_packages("shaderc")
    if is_plat("windows", "mingw") and is_mode("debug") then
        add_links("shaderc_combinedd")
    else
        add_links("shaderc_combined")
    end

    add_defines("RENDER_API_VULKAN")
    if is_plat("windows") then
        add_links("vulkan-1")
    elseif is_plat("linux") or is_plat("macosx") then
        add_links("vulkan")
    end
end

if has_config("directx") then
    add_defines("RENDER_API_DIRECTX")
    if is_plat("windows") then
        add_links("d3d11", "dxgi", "d3dcompiler")
    else
        print("Warning: DirectX is only available on Windows")
    end
end

-- Platform-specific settings
if is_plat("windows") then
    add_syslinks("user32", "gdi32", "shell32")
elseif is_plat("macosx") then
    add_frameworks("Cocoa", "IOKit", "CoreVideo")
end

set_languages("c++latest")
set_rundir("$(projectdir)")

target("Engine")
    set_kind("shared")
    add_defines("ENGINE_EXPORTS")
    
    add_files("Engine/src/**.cpp")
    add_headerfiles("Engine/src/**.h", "Engine/src/**.hpp")
    add_includedirs("Engine/src")

    -- Always add base packages
    add_packages("galaxymath", "stb", "thread-pool")
    
    add_cxxflags("-Wall", "-Wextra")
    
    -- Validation: Ensure at least one window API is enabled
    after_load(function (target)
        if not has_config("glfw") and not has_config("sdl") then
            raise("Error: At least one window API (GLFW or SDL) must be enabled!")
        end
        
        if not has_config("opengl") and not has_config("vulkan") and not has_config("directx") then
            raise("Error: At least one render API (OpenGL, Vulkan, or DirectX) must be enabled!")
        end
        
        if has_config("directx") and not is_plat("windows") then
            raise("Error: DirectX is only available on Windows!")
        end
    end)
target_end()

target("Editor")
    set_default(true)
    set_kind("binary")
    
    add_deps("Engine")
    
    add_files("Editor/src/**.cpp")
    add_headerfiles("Editor/src/**.h", "Editor/src/**.hpp")
    add_includedirs("Editor/src",  "Engine/src")
    
    add_packages("imgui", "galaxymath", "stb", "thread-pool")
target_end()