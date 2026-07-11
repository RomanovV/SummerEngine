#include "Assets/ModelLoader.h"
#include "Core/Log.h"
#include "Core/Paths.h"
#include "Gfx/GpuCapture.h"
#include "Gfx/Renderer.h"
#include "Platform/Window.h"

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <string>

int main() {
    summer::Log::Init();
    spdlog::info("SummerEngine sandbox starting");

    // Must run before the D3D12 device is created so PIX can attach to a standalone launch.
    summer::gfx::GpuCapture::LoadCaptureTools();

    const std::string modelPath = summer::Paths::Resolve("assets/models/cube.obj");
    summer::assets::MeshData meshData;
    if (!summer::assets::ModelLoader::LoadFromFile(modelPath.c_str(), meshData)) {
        spdlog::critical("Failed to load default model '{}'", modelPath);
        return 1;
    }

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

    if (!renderer.SetMesh(meshData)) {
        spdlog::critical("Failed to upload mesh to GPU");
        return 1;
    }

    // Associate RenderDoc's overlay/capture with our device + window (no-op without RenderDoc).
    summer::gfx::GpuCapture::SetActiveWindow(
        renderer.GetNativeDevice(), window.GetNativeHandle());

    float clearColor[4] = {0.1f, 0.1f, 0.15f, 1.0f};
    float meshColor[4] = {0.85f, 0.55f, 0.25f, 1.0f};
    renderer.SetClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    renderer.SetMeshColor(meshColor[0], meshColor[1], meshColor[2], meshColor[3]);

    while (!window.ShouldClose()) {
        window.PollEvents();

        if (window.WasResized()) {
            renderer.OnResize(
                static_cast<uint32_t>(window.GetWidth()),
                static_cast<uint32_t>(window.GetHeight()));
            window.ClearResizedFlag();
        }

        renderer.BeginImGuiFrame();

        ImGui::Begin("Summer Sandbox");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("Resolution: %d x %d", window.GetWidth(), window.GetHeight());
        if (ImGui::ColorEdit4("Clear Color", clearColor)) {
            renderer.SetClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        }
        if (ImGui::ColorEdit4("Mesh Color", meshColor)) {
            renderer.SetMeshColor(meshColor[0], meshColor[1], meshColor[2], meshColor[3]);
        }

        ImGui::Separator();
        ImGui::Text("Loaded mesh: %s", meshData.sourcePath.c_str());
        ImGui::Text("Vertices: %u", meshData.vertexCount());
        ImGui::Text("Indices: %u", meshData.indexCount());
        ImGui::Text("SubMeshes: %zu", meshData.subMeshes.size());

        ImGui::Separator();
        ImGui::Text("PIX: %s", summer::gfx::GpuCapture::IsPixLoaded() ? "attached" : "not loaded");
        ImGui::Text(
            "RenderDoc: %s",
            summer::gfx::GpuCapture::IsRenderDocLoaded() ? "connected" : "not loaded");
        if (summer::gfx::GpuCapture::IsRenderDocLoaded()) {
            if (ImGui::Button("Capture frame (F11)")) {
                summer::gfx::GpuCapture::TriggerRenderDocCapture();
            }
        }
        ImGui::End();

        if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
            summer::gfx::GpuCapture::TriggerRenderDocCapture();
        }

        renderer.RenderFrame();
    }

    renderer.Shutdown();
    spdlog::info("Shutting down");
    return 0;
}
