#include "Gfx/Renderer.h"
#include "Gfx/Helpers.h"

#include <spdlog/spdlog.h>

namespace summer::gfx {

Renderer::~Renderer() {
    Shutdown();
}

bool Renderer::Initialize(Window& window, bool enableDebugLayer) {
    if (!m_device.Initialize(enableDebugLayer)) {
        return false;
    }

    if (!m_swapChain.Initialize(
            m_device,
            window.GetNativeHandle(),
            static_cast<uint32_t>(window.GetWidth()),
            static_cast<uint32_t>(window.GetHeight()))) {
        m_device.Shutdown();
        return false;
    }

    m_initialized = true;
    spdlog::info("Renderer initialized");
    return true;
}

void Renderer::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_device.WaitForGpu();
    m_swapChain.Shutdown();
    m_device.Shutdown();
    m_initialized = false;
}

void Renderer::OnResize(uint32_t width, uint32_t height) {
    if (!m_initialized || width == 0 || height == 0) {
        return;
    }

    m_device.WaitForGpu();
    m_swapChain.Resize(m_device, width, height);
}

void Renderer::SetClearColor(float r, float g, float b, float a) {
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
}

void Renderer::TransitionResource(
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter) {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = stateBefore;
    barrier.Transition.StateAfter = stateAfter;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_device.GetCommandList()->ResourceBarrier(1, &barrier);
}

void Renderer::RenderFrame() {
    if (!m_initialized) {
        return;
    }

    const uint32_t backBufferIndex = m_swapChain.GetCurrentBackBufferIndex();
    m_device.WaitForBackBuffer(backBufferIndex);

    FrameContext& frame = m_device.GetFrame(backBufferIndex);
    ID3D12Resource* backBuffer = m_swapChain.GetBackBuffer(backBufferIndex);
    if (!backBuffer) {
        return;
    }

    if (!CheckHRESULT(frame.CommandAllocator->Reset(), "Reset command allocator")) {
        return;
    }

    if (!CheckHRESULT(
            m_device.GetCommandList()->Reset(frame.CommandAllocator, nullptr),
            "Reset command list")) {
        return;
    }

    TransitionResource(
        backBuffer,
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    const D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain.GetRTV(backBufferIndex);
    m_device.GetCommandList()->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    m_device.GetCommandList()->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);

    TransitionResource(
        backBuffer,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);

    if (!CheckHRESULT(m_device.GetCommandList()->Close(), "Close command list")) {
        return;
    }

    ID3D12CommandList* commandLists[] = {m_device.GetCommandList()};
    m_device.GetCommandQueue()->ExecuteCommandLists(1, commandLists);

    m_swapChain.Present();
    m_device.SignalBackBuffer(backBufferIndex);
}

} // namespace summer::gfx
