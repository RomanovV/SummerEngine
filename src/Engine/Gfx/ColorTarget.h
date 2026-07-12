#pragma once

#include "Gfx/GpuDescriptorHeap.h"

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace summer::gfx {

class Device;

// An offscreen color render target that is also sampleable (SRV). Used as the HDR scene target
// that the tonemap pass resolves to the swapchain. Owns a private 1-slot RTV heap and borrows an
// SRV slot from the shared descriptor heap.
class ColorTarget {
public:
    ColorTarget() = default;
    ~ColorTarget();

    ColorTarget(const ColorTarget&) = delete;
    ColorTarget& operator=(const ColorTarget&) = delete;

    bool Initialize(
        Device& device,
        GpuDescriptorHeap& srvHeap,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_R16G16B16A16_FLOAT);
    void Shutdown();

    // Caller must ensure the GPU is idle (Device::WaitForGpu) before resizing.
    bool Resize(Device& device, uint32_t width, uint32_t height);

    ID3D12Resource* Resource() const { return m_resource; }
    DXGI_FORMAT Format() const { return m_format; }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return m_rtv; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpu() const { return m_srv.gpu; }

private:
    bool CreateResourceAndViews(Device& device);

    GpuDescriptorHeap* m_srvHeap = nullptr;
    GpuDescriptorHeap::Handle m_srv;
    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtv{};
    ID3D12Resource* m_resource = nullptr;
    DXGI_FORMAT m_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace summer::gfx
