workspace "SummerEngine"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "Sandbox"
    location "build"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

    filter "configurations:Debug"
        defines { "SUMMER_DEBUG" }
        symbols "On"
        runtime "Debug"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        runtime "Release"

    filter {}

-- Vendor: GLFW
project "GLFW"
    kind "StaticLib"
    language "C"
    location "build"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"

    files {
        "vendor/glfw/include/GLFW/glfw3.h",
        "vendor/glfw/include/GLFW/glfw3native.h",
        "vendor/glfw/src/**.h",
        "vendor/glfw/src/context.c",
        "vendor/glfw/src/init.c",
        "vendor/glfw/src/input.c",
        "vendor/glfw/src/monitor.c",
        "vendor/glfw/src/platform.c",
        "vendor/glfw/src/vulkan.c",
        "vendor/glfw/src/window.c",
        "vendor/glfw/src/egl_context.c",
        "vendor/glfw/src/osmesa_context.c",
        "vendor/glfw/src/null_init.c",
        "vendor/glfw/src/null_monitor.c",
        "vendor/glfw/src/null_window.c",
        "vendor/glfw/src/null_joystick.c"
    }

    includedirs { "vendor/glfw/include" }

    filter "system:windows"
        defines { "_GLFW_WIN32", "UNICODE", "_UNICODE", "_CRT_SECURE_NO_WARNINGS" }
        files {
            "vendor/glfw/src/win32_module.c",
            "vendor/glfw/src/win32_time.c",
            "vendor/glfw/src/win32_thread.c",
            "vendor/glfw/src/win32_init.c",
            "vendor/glfw/src/win32_joystick.c",
            "vendor/glfw/src/win32_monitor.c",
            "vendor/glfw/src/win32_window.c",
            "vendor/glfw/src/wgl_context.c"
        }

    filter {}

-- Vendor: Dear ImGui
project "ImGui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    location "build"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"

    files {
        "vendor/imgui/imgui.cpp",
        "vendor/imgui/imgui_draw.cpp",
        "vendor/imgui/imgui_tables.cpp",
        "vendor/imgui/imgui_widgets.cpp",
        "vendor/imgui/backends/imgui_impl_glfw.cpp",
        "vendor/imgui/backends/imgui_impl_dx12.cpp"
    }

    includedirs {
        "vendor/imgui",
        "vendor/imgui/backends",
        "vendor/glfw/include"
    }

    filter "system:windows"
        links { "d3d12", "dxgi" }

    filter {}

-- Engine static library
project "Engine"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    location "build"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"

    files {
        "src/Engine/**.h",
        "src/Engine/**.cpp"
    }

    includedirs {
        "src/Engine",
        "vendor/spdlog/include",
        "vendor/glfw/include",
        "vendor/stb",
        "vendor/imgui",
        "vendor/imgui/backends",
        "vendor/assimp/include",
        "vendor/assimp/build/include",
        "vendor/renderdoc"
    }

    links { "GLFW", "ImGui" }

    filter "configurations:Debug"
        libdirs {
            "vendor/assimp/build/lib/Debug",
            "vendor/assimp/build/contrib/zlib/Debug"
        }
        links { "assimp-vc143-mtd", "zlibstaticd" }

    filter "configurations:Release"
        libdirs {
            "vendor/assimp/build/lib/Release",
            "vendor/assimp/build/contrib/zlib/Release"
        }
        links { "assimp-vc143-mt", "zlibstatic" }

    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS" }
        links { "d3d12", "dxgi", "dxguid", "user32", "gdi32", "shell32", "ole32", "advapi32", "winmm", "setupapi" }

    filter {}

-- Sandbox executable
project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    location "build"
    targetdir "build/bin/%{cfg.buildcfg}"
    objdir "build/obj/%{prj.name}/%{cfg.buildcfg}"

    files {
        "src/Sandbox/**.cpp"
    }

    includedirs {
        "src/Engine",
        "vendor/spdlog/include",
        "vendor/imgui"
    }

    links { "Engine" }

    -- Assets/shaders are staged next to the exe (postbuild), and paths resolve relative to the
    -- executable, so the app runs identically from the IDE or when launched standalone by PIX/RenderDoc.
    debugdir "%{cfg.targetdir}"

    filter "configurations:Debug"
        prebuildcommands {
            'powershell -ExecutionPolicy Bypass -File "%{wks.location}/../scripts/compile_shaders.ps1" -Config Debug'
        }
        postbuildcommands {
            'powershell -ExecutionPolicy Bypass -File "%{wks.location}/../scripts/stage_assets.ps1" -Config Debug'
        }

    filter "configurations:Release"
        prebuildcommands {
            'powershell -ExecutionPolicy Bypass -File "%{wks.location}/../scripts/compile_shaders.ps1" -Config Release'
        }
        postbuildcommands {
            'powershell -ExecutionPolicy Bypass -File "%{wks.location}/../scripts/stage_assets.ps1" -Config Release'
        }

    filter "system:windows"
        links { "user32", "gdi32", "shell32", "ole32", "advapi32", "winmm", "setupapi" }

    filter {}
