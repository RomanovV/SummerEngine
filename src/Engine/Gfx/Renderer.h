#pragma once

#include "Gfx/Device.h"
#include "Gfx/SwapChain.h"
#include "Platform/Window.h"

#include <cstdint>

namespace summer::gfx {

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Initialize(Window& window, bool enableDebugLayer = true);
    void Shutdown();

    void OnResize(uint32_t width, uint32_t height);
    void RenderFrame();

    void SetClearColor(float r, float g, float b, float a);

private:
    void TransitionResource(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter);

    Device m_device;
    SwapChain m_swapChain;
    float m_clearColor[4] = {0.1f, 0.1f, 0.15f, 1.0f};
    bool m_initialized = false;
};

} // namespace summer::gfx
