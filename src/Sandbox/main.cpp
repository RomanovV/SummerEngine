#include "Core/Log.h"
#include "Gfx/Renderer.h"
#include "Platform/Window.h"

#include <imgui.h>
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

    // Shared with the settings panel; drives the per-frame clear color.
    float clearColor[4] = {0.1f, 0.1f, 0.15f, 1.0f};
    renderer.SetClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    while (!window.ShouldClose()) {
        window.PollEvents();

        if (window.WasResized()) {
            renderer.OnResize(
                static_cast<uint32_t>(window.GetWidth()),
                static_cast<uint32_t>(window.GetHeight()));
            window.ClearResizedFlag();
        }

        // --- ImGui UI (add your experiment toggles here) ---
        renderer.BeginImGuiFrame();

        ImGui::Begin("Summer Sandbox");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Resolution: %d x %d", window.GetWidth(), window.GetHeight());
        if (ImGui::ColorEdit4("Clear Color", clearColor)) {
            renderer.SetClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        }
        ImGui::End();

        renderer.RenderFrame();
    }

    renderer.Shutdown();
    spdlog::info("Shutting down");
    return 0;
}
