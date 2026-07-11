#pragma once

#include "Gfx/Device.h"
#include "Gfx/ImGuiLayer.h"
#include "Gfx/SwapChain.h"
#include "Platform/Window.h"

#include <cstdint>

namespace summer::gfx {

// High-level rendering orchestrator.
// Owns the D3D12 device, swap chain, and ImGui layer; records one frame of GPU work per call.
class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Initialize(Window& window, bool enableDebugLayer = true);
    void Shutdown();

    void OnResize(uint32_t width, uint32_t height);

    // ImGui frame lifecycle:
    //   1. BeginImGuiFrame()
    //   2. Build widgets (typically in Sandbox/main.cpp)
    //   3. RenderFrame()  -- records GPU commands, presents
    void BeginImGuiFrame();
    void RenderFrame();

    void SetClearColor(float r, float g, float b, float a);

private:
    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter);

    Device m_device;
    SwapChain m_swapChain;
    ImGuiLayer m_imguiLayer;
    float m_clearColor[4] = {0.1f, 0.1f, 0.15f, 1.0f};
    bool m_initialized = false;
};

} // namespace summer::gfx
