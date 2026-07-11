#include "Core/Log.h"
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

    spdlog::info("Native HWND: {}", static_cast<void*>(window.GetNativeHandle()));

    while (!window.ShouldClose()) {
        window.PollEvents();

        if (window.WasResized()) {
            spdlog::info("Window resized: {}x{}", window.GetWidth(), window.GetHeight());
            window.ClearResizedFlag();
        }
    }

    spdlog::info("Shutting down");
    return 0;
}
