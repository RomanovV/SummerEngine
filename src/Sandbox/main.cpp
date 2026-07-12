#include "Assets/Image.h"
#include "Assets/Primitives.h"
#include "Core/Log.h"
#include "Core/Paths.h"
#include "Gfx/GpuCapture.h"
#include "Gfx/Renderer.h"
#include "Gfx/Texture2D.h"
#include "Platform/Window.h"
#include "Scene/Scene.h"

#include <DirectXMath.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <string>

using namespace DirectX;

namespace {

// Places a model in the scene at a position with uniform scale.
void AddInstance(
    summer::scene::Scene& scene,
    summer::gfx::Model* model,
    const XMFLOAT3& position,
    float scale) {
    const XMMATRIX world =
        XMMatrixScaling(scale, scale, scale) *
        XMMatrixTranslation(position.x, position.y, position.z);
    scene.AddEntity(model, world);
}

} // namespace

int main() {
    summer::Log::Init();
    spdlog::info("SummerEngine sandbox starting");

    // Must run before the D3D12 device is created so PIX can attach to a standalone launch.
    summer::gfx::GpuCapture::LoadCaptureTools();

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

    summer::gfx::GpuCapture::SetActiveWindow(
        renderer.GetNativeDevice(), window.GetNativeHandle());

    // Procedural textures: a subtle floor grid and a bolder cube checker.
    summer::gfx::Texture2D* floorTexture = renderer.CreateTexture(
        summer::assets::Image::MakeCheckerboard(256, 16, 0xFF6E6E6E, 0xFF4A4A4A));
    summer::gfx::Texture2D* cubeTexture = renderer.CreateTexture(
        summer::assets::Image::MakeCheckerboard(256, 4, 0xFFDFA24A, 0xFF2C2C34));

    // Ground plane (procedural) + cubes (loaded via Assimp to exercise the mesh loader).
    summer::gfx::Model* planeModel =
        renderer.CreateModel(summer::assets::Primitives::MakePlane(12.0f, 12.0f), floorTexture);

    const std::string cubePath = summer::Paths::Resolve("assets/models/cube.obj");
    summer::gfx::Model* cubeModel = renderer.LoadModel(cubePath, cubeTexture);
    if (!planeModel || !cubeModel) {
        spdlog::critical("Failed to create scene models");
        return 1;
    }

    summer::scene::Scene scene;
    AddInstance(scene, planeModel, {0.0f, 0.0f, 0.0f}, 1.0f);
    AddInstance(scene, cubeModel, {0.0f, 1.0f, 0.0f}, 1.0f);
    AddInstance(scene, cubeModel, {-3.5f, 0.75f, 1.5f}, 0.75f);
    AddInstance(scene, cubeModel, {3.0f, 0.5f, -2.0f}, 0.5f);
    AddInstance(scene, cubeModel, {1.5f, 1.5f, 3.0f}, 1.5f);

    summer::scene::Camera& camera = scene.GetCamera();
    camera.SetTarget({0.0f, 1.0f, 0.0f});
    camera.SetDistance(14.0f);

    float clearColor[4] = {0.08f, 0.09f, 0.12f, 1.0f};
    renderer.SetClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);

    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    window.GetCursorPosition(lastMouseX, lastMouseY);

    while (!window.ShouldClose()) {
        window.PollEvents();

        if (window.WasResized()) {
            renderer.OnResize(
                static_cast<uint32_t>(window.GetWidth()),
                static_cast<uint32_t>(window.GetHeight()));
            window.ClearResizedFlag();
        }

        // --- Orbit camera controls (gated so ImGui interactions don't move the camera) ---
        double mouseX = 0.0;
        double mouseY = 0.0;
        window.GetCursorPosition(mouseX, mouseY);
        const float dx = static_cast<float>(mouseX - lastMouseX);
        const float dy = static_cast<float>(mouseY - lastMouseY);
        lastMouseX = mouseX;
        lastMouseY = mouseY;

        const ImGuiIO& io = ImGui::GetIO();
        if (!io.WantCaptureMouse) {
            if (window.IsMouseButtonDown(summer::MouseButton::Left)) {
                camera.Orbit(dx * 0.005f, -dy * 0.005f);
            }
            if (window.IsMouseButtonDown(summer::MouseButton::Middle) ||
                window.IsMouseButtonDown(summer::MouseButton::Right)) {
                camera.Pan(dx, dy);
            }
            const double scroll = window.ConsumeScrollDelta();
            if (scroll != 0.0) {
                camera.Zoom(static_cast<float>(scroll));
            }
        }

        if (!io.WantCaptureKeyboard) {
            // WASD/QE nudge the orbit target for quick framing.
            const float panStep = 40.0f;
            if (window.IsKeyDown(summer::Key::A)) camera.Pan(panStep, 0.0f);
            if (window.IsKeyDown(summer::Key::D)) camera.Pan(-panStep, 0.0f);
            if (window.IsKeyDown(summer::Key::W)) camera.Pan(0.0f, -panStep);
            if (window.IsKeyDown(summer::Key::S)) camera.Pan(0.0f, panStep);
            if (window.IsKeyDown(summer::Key::E)) camera.Zoom(0.15f);
            if (window.IsKeyDown(summer::Key::Q)) camera.Zoom(-0.15f);
        }

        renderer.BeginImGuiFrame();

        ImGui::Begin("Summer Sandbox");
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Resolution: %d x %d", window.GetWidth(), window.GetHeight());
        if (ImGui::ColorEdit4("Clear Color", clearColor)) {
            renderer.SetClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Camera: LMB orbit, MMB/RMB pan, scroll zoom, WASD/QE keys");
        const XMFLOAT3 camPos = camera.GetPosition();
        ImGui::Text("Position: %.2f, %.2f, %.2f", camPos.x, camPos.y, camPos.z);
        ImGui::Text("Entities: %zu", scene.GetEntities().size());

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

        renderer.RenderFrame(scene);
    }

    renderer.Shutdown();
    spdlog::info("Shutting down");
    return 0;
}
