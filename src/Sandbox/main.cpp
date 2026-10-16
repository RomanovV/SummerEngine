#include "Assets/Image.h"
#include "Assets/Primitives.h"
#include "Core/Log.h"
#include "Core/Paths.h"
#include "Gfx/DebugView.h"
#include "Gfx/GpuCapture.h"
#include "Gfx/Model.h"
#include "Gfx/Renderer.h"
#include "Gfx/Texture2D.h"
#include "Gfx/TonemapPipeline.h"
#include "Platform/Window.h"
#include "Scene/Scene.h"

#include <DirectXMath.h>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <string>

using namespace DirectX;

namespace {

void AddInstance(
    summer::scene::Scene& scene,
    summer::gfx::Model* model,
    const XMFLOAT3& position,
    float scale,
    float yawRadians = 0.0f) {
    const XMMATRIX world =
        XMMatrixScaling(scale, scale, scale) *
        XMMatrixRotationY(yawRadians) *
        XMMatrixTranslation(position.x, position.y, position.z);
    scene.AddEntity(model, world);
}

// Overrides every material's base color of a procedurally-created model (no imported textures).
void SetModelBaseColor(summer::gfx::Model* model, summer::gfx::Texture2D* texture) {
    for (summer::gfx::Material& material : model->Materials()) {
        material.baseColor = texture;
    }
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

    summer::gfx::GpuCapture::SetActiveWindow(renderer.GetNativeDevice(), window.GetNativeHandle());

    // Procedural checkerboards for the plane + cubes (helmet brings its own textures).
    summer::gfx::Texture2D* floorTexture = renderer.CreateTexture(
        summer::assets::Image::MakeCheckerboard(256, 16, 0xFF6E6E6E, 0xFF4A4A4A));
    summer::gfx::Texture2D* cubeTexture = renderer.CreateTexture(
        summer::assets::Image::MakeCheckerboard(256, 4, 0xFFDFA24A, 0xFF2C2C34));

    summer::gfx::Model* planeModel =
        renderer.CreateModel(summer::assets::Primitives::MakePlane(12.0f, 12.0f));
    summer::gfx::Model* cubeModel =
        renderer.LoadModel(summer::Paths::Resolve("assets/models/cube.obj"));
    summer::gfx::Model* helmetModel =
        renderer.LoadModel(summer::Paths::Resolve("assets/models/FlightHelmet/glTF/FlightHelmet.gltf"));

    if (!planeModel || !cubeModel) {
        spdlog::critical("Failed to create base scene models");
        return 1;
    }
    if (!helmetModel) {
        spdlog::warn("FlightHelmet not loaded; continuing without it");
    }

    SetModelBaseColor(planeModel, floorTexture);
    SetModelBaseColor(cubeModel, cubeTexture);

    summer::scene::Scene scene;
    AddInstance(scene, planeModel, {0.0f, 0.0f, 0.0f}, 1.0f);
    if (helmetModel) {
        // Face the default camera: MakeLeftHanded flips imported models to face -Z.
        AddInstance(scene, helmetModel, {0.0f, 0.0f, 0.0f}, 6.0f, XM_PI);
    }
    AddInstance(scene, cubeModel, {-3.5f, 0.75f, 1.5f}, 0.75f);
    AddInstance(scene, cubeModel, {3.0f, 0.5f, -2.0f}, 0.5f);
    AddInstance(scene, cubeModel, {3.2f, 1.0f, 2.4f}, 1.0f);

    summer::scene::Camera& camera = scene.GetCamera();
    camera.SetTarget({0.0f, 1.4f, 0.0f});
    camera.SetDistance(8.0f);
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
            const float panStep = 40.0f;
            if (window.IsKeyDown(summer::Key::A)) camera.Pan(panStep, 0.0f);
            if (window.IsKeyDown(summer::Key::D)) camera.Pan(-panStep, 0.0f);
            if (window.IsKeyDown(summer::Key::W)) camera.Pan(0.0f, -panStep);
            if (window.IsKeyDown(summer::Key::S)) camera.Pan(0.0f, panStep);
            if (window.IsKeyDown(summer::Key::E)) camera.Zoom(0.15f);
            if (window.IsKeyDown(summer::Key::Q)) camera.Zoom(-0.15f);
        }

        renderer.BeginImGuiFrame();

        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushFont(nullptr, style.FontSizeBase * 2.0f);
        ImGui::PushStyleVar(
            ImGuiStyleVar_FramePadding,
            ImVec2(style.FramePadding.x * 2.0f, style.FramePadding.y * 2.0f));
        ImGui::PushStyleVar(
            ImGuiStyleVar_ItemSpacing,
            ImVec2(style.ItemSpacing.x * 1.5f, style.ItemSpacing.y * 2.0f));

        float menuBarHeight = ImGui::GetFrameHeight();
        if (ImGui::BeginMainMenuBar()) {
            menuBarHeight = ImGui::GetWindowSize().y;
            if (ImGui::BeginMenu("View")) {
                const bool litSelected = renderer.GetDebugView() == summer::gfx::DebugView::Lit;
                if (ImGui::MenuItem("Lit", nullptr, litSelected)) {
                    renderer.SetDebugView(summer::gfx::DebugView::Lit);
                }
                if (ImGui::BeginMenu("GBuffer")) {
                    const summer::gfx::DebugView gbufferViews[] = {
                        summer::gfx::DebugView::GBufferOverview,
                        summer::gfx::DebugView::GBufferAlbedo,
                        summer::gfx::DebugView::GBufferNormal,
                        summer::gfx::DebugView::GBufferOcclusion,
                        summer::gfx::DebugView::GBufferMetallic,
                        summer::gfx::DebugView::GBufferRoughness,
                        summer::gfx::DebugView::GBufferEmissive,
                        summer::gfx::DebugView::GBufferDepth,
                    };
                    for (summer::gfx::DebugView view : gbufferViews) {
                        const bool selected = renderer.GetDebugView() == view;
                        if (ImGui::MenuItem(summer::gfx::DebugViewLabel(view), nullptr, selected)) {
                            renderer.SetDebugView(view);
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar(2);
        ImGui::PopFont();

        if (renderer.GetDebugView() != summer::gfx::DebugView::Lit) {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            const float menuH = menuBarHeight;
            auto stamp = [&](const char* text, float x, float y) {
                const ImVec2 pad(6.0f, 3.0f);
                const ImVec2 size = ImGui::CalcTextSize(text);
                const float labelY = (y < menuH + 1.0f) ? menuH + 4.0f : y + 4.0f;
                const ImVec2 pmin(x + 6.0f, labelY);
                const ImVec2 pmax(pmin.x + size.x + pad.x * 2.0f, pmin.y + size.y + pad.y * 2.0f);
                drawList->AddRectFilled(pmin, pmax, IM_COL32(0, 0, 0, 160), 3.0f);
                drawList->AddText(
                    ImVec2(pmin.x + pad.x, pmin.y + pad.y), IM_COL32(255, 255, 255, 230), text);
            };

            if (renderer.GetDebugView() == summer::gfx::DebugView::GBufferOverview) {
                const uint32_t width = static_cast<uint32_t>(window.GetWidth());
                const uint32_t height = static_cast<uint32_t>(window.GetHeight());
                summer::gfx::DebugViewTile tiles[summer::gfx::kGBufferOverviewTileCount];
                summer::gfx::FillGBufferOverviewTiles(width, height, tiles);
                const float cellW = static_cast<float>(width) / static_cast<float>(summer::gfx::kGBufferOverviewGrid);
                const float cellH = static_cast<float>(height) / static_cast<float>(summer::gfx::kGBufferOverviewGrid);
                stamp("Lit", cellW * 2.0f, cellH * 2.0f);
                for (uint32_t i = 0; i < summer::gfx::kGBufferOverviewTileCount; ++i) {
                    stamp(summer::gfx::DebugViewLabel(tiles[i].view), tiles[i].x, tiles[i].y);
                }
            } else {
                stamp(summer::gfx::DebugViewLabel(renderer.GetDebugView()), 0.0f, 0.0f);
            }
        }

        ImGui::Begin("Summer Sandbox");
        ImGui::Text("FPS: %.1f", io.Framerate);
        ImGui::Text("Resolution: %d x %d", window.GetWidth(), window.GetHeight());
        const XMFLOAT3 camPos = camera.GetPosition();
        ImGui::Text("Camera: %.2f, %.2f, %.2f", camPos.x, camPos.y, camPos.z);
        ImGui::TextUnformatted("LMB orbit, MMB/RMB pan, scroll zoom, WASD/QE");

        if (ImGui::CollapsingHeader("Tonemap", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Exposure", &renderer.Exposure(), 0.05f, 8.0f, "%.2f");
            int op = static_cast<int>(renderer.TonemapOperator());
            if (ImGui::Combo("Operator", &op, "Reinhard\0ACES\0")) {
                renderer.TonemapOperator() = static_cast<summer::gfx::TonemapPipeline::Operator>(op);
            }
        }

        if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
            summer::gfx::SceneLighting& lighting = renderer.Lighting();
            ImGui::ColorEdit3("Ambient", &lighting.ambient.x);
            ImGui::Separator();
            ImGui::TextUnformatted("Directional");
            ImGui::SliderFloat3("Direction", &lighting.directional.direction.x, -1.0f, 1.0f);
            ImGui::ColorEdit3("Dir Color", &lighting.directional.color.x);
            ImGui::SliderFloat("Dir Intensity", &lighting.directional.intensity, 0.0f, 10.0f);
            ImGui::Separator();
            ImGui::SliderInt("Point Lights", &lighting.pointCount, 0, summer::gfx::kMaxPointLights);
            for (int i = 0; i < lighting.pointCount; ++i) {
                ImGui::PushID(i);
                ImGui::Text("Point %d", i);
                ImGui::SliderFloat3("Pos", &lighting.points[i].position.x, -15.0f, 15.0f);
                ImGui::ColorEdit3("Color", &lighting.points[i].color.x);
                ImGui::SliderFloat("Intensity", &lighting.points[i].intensity, 0.0f, 100.0f);
                ImGui::SliderFloat("Radius", &lighting.points[i].radius, 1.0f, 50.0f);
                ImGui::PopID();
            }
        }

        if (helmetModel && ImGui::CollapsingHeader("Helmet Materials")) {
            int index = 0;
            for (summer::gfx::Material& material : helmetModel->Materials()) {
                ImGui::PushID(index);
                ImGui::Text("Material %d", index);
                ImGui::SliderFloat("Metallic", &material.metallicFactor, 0.0f, 1.0f);
                ImGui::SliderFloat("Roughness", &material.roughnessFactor, 0.0f, 1.0f);
                ImGui::PopID();
                ++index;
            }
        }

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
