#include "Core/Log.h"
#include "Gfx/Renderer.h"
#include "Platform/Window.h"

#include <spdlog/spdlog.h>

int main() {
    summer::Log::Init();
    spdlog::info("SummerEngine sandbox starting");

    summer::WindowConfig config;
    config.title = "Summer Sandbox";

    summer::Window window(config);
    if (!window.GetGLFWHandle()) {
        spdlog::critical("Failed to create application window");
        return 1;
    }

    summer::gfx::Renderer renderer;
#if defined(SUMMER_DEBUG)
    constexpr bool enableDebugLayer = true;
#else
    constexpr bool enableDebugLayer = false;
#endif

    if (!renderer.Initialize(window, enableDebugLayer)) {
        spdlog::critical("Failed to initialize renderer");
        return 1;
    }

    while (!window.ShouldClose()) {
        window.PollEvents();

        if (window.WasResized()) {
            renderer.OnResize(
                static_cast<uint32_t>(window.GetWidth()),
                static_cast<uint32_t>(window.GetHeight()));
            window.ClearResizedFlag();
        }

        renderer.RenderFrame();
    }

    renderer.Shutdown();
    spdlog::info("Shutting down");
    return 0;
}
