#pragma once

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace summer::gfx {

// Must match swap chain BufferCount. Each back buffer gets its own command allocator
// and fence tracking so the CPU can record the next frame while the GPU is still busy.
static constexpr uint32_t FrameCount = 2;

struct FrameContext {
    ID3D12CommandAllocator* CommandAllocator = nullptr;
};

// Owns core D3D12 objects: device, command queue, command list, and GPU fence.
// Does NOT own the swap chain -- that lives in SwapChain.
class Device {
public:
    Device() = default;
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    bool Initialize(bool enableDebugLayer = true);
    void Shutdown();

    // Blocks until all submitted GPU work completes. Used during shutdown and resize.
    void WaitForGpu();

    // Per-back-buffer sync: wait before reusing a buffer the GPU may still be reading.
    void WaitForBackBuffer(uint32_t backBufferIndex);
    void SignalBackBuffer(uint32_t backBufferIndex);

    FrameContext& GetFrame(uint32_t backBufferIndex) { return m_frames[backBufferIndex]; }

    ID3D12Device* GetD3D12Device() const { return m_device; }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue; }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList; }
    IDXGIFactory4* GetFactory() const { return m_factory; }

private:
    IDXGIFactory4* m_factory = nullptr;
    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    ID3D12GraphicsCommandList* m_commandList = nullptr;
    ID3D12Fence* m_fence = nullptr;
    HANDLE m_fenceEvent = nullptr;

    FrameContext m_frames[FrameCount]{};
    uint64_t m_fenceValues[FrameCount]{}; // Last signaled fence value per back buffer
    uint64_t m_fenceCounter = 0;
};

} // namespace summer::gfx
