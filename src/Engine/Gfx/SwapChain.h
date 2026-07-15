#pragma once

#include "Gfx/Device.h"

#include <dxgi1_6.h>
#include <wrl/client.h>

#include <cstdint>

struct HWND__;
using HWND = HWND__*;

namespace summer::gfx {

// DXGI swap chain + back-buffer RTVs. Depth now lives in a standalone DepthStencil owned by the
// Renderer (the back buffers are DXGI-owned and recreated on resize, so their RTVs stay local).
class SwapChain {
public:
    SwapChain() = default;
    ~SwapChain();

    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

    bool Initialize(Device& device, HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    // Caller must call Device::WaitForGpu() before Resize.
    bool Resize(Device& device, uint32_t width, uint32_t height);

    void Present();
    uint32_t GetCurrentBackBufferIndex() const;

    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

    DXGI_FORMAT GetFormat() const { return m_format; }

    ID3D12Resource* GetBackBuffer(uint32_t index) const;

    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(uint32_t index) const;

private:
    bool CreateRenderTargets(Device& device);

    HWND m_hwnd = nullptr;
    DXGI_FORMAT m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    uint32_t m_rtvDescriptorSize = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
};

} // namespace summer::gfx
