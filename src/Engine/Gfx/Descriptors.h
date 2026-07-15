#pragma once

#include "Gfx/CpuDescriptorHeap.h"
#include "Gfx/GpuDescriptorHeap.h"

#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace summer::gfx {

// Thin RAII wrappers around a single descriptor slot. Each Create*() call borrows a slot from a
// heap (a global manager) and writes the matching view for a resource; Release() returns the slot.
// These abstract "descriptor handle creation" so RenderTarget/DepthStencil don't touch heaps or
// D3D12_*_VIEW_DESC directly.

// Shader-visible SRV, allocated from the shared GpuDescriptorHeap.
class SRVDescriptor {
public:
    SRVDescriptor() = default;
    ~SRVDescriptor();

    SRVDescriptor(const SRVDescriptor&) = delete;
    SRVDescriptor& operator=(const SRVDescriptor&) = delete;

    bool CreateTexture2D(
        ID3D12Device* device,
        GpuDescriptorHeap& heap,
        ID3D12Resource* resource,
        DXGI_FORMAT viewFormat,
        uint32_t mipLevels = 1);
    void Release();

    bool IsValid() const { return m_handle.IsValid(); }
    D3D12_CPU_DESCRIPTOR_HANDLE Cpu() const { return m_handle.cpu; }
    D3D12_GPU_DESCRIPTOR_HANDLE Gpu() const { return m_handle.gpu; }

private:
    GpuDescriptorHeap* m_heap = nullptr;
    GpuDescriptorHeap::Handle m_handle;
};

// Render-target view, allocated from a CPU-only RTV heap.
class RTVDescriptor {
public:
    RTVDescriptor() = default;
    ~RTVDescriptor();

    RTVDescriptor(const RTVDescriptor&) = delete;
    RTVDescriptor& operator=(const RTVDescriptor&) = delete;

    bool Create(
        ID3D12Device* device,
        CpuDescriptorHeap& heap,
        ID3D12Resource* resource,
        DXGI_FORMAT viewFormat);
    void Release();

    bool IsValid() const { return m_index != CpuDescriptorHeap::kInvalidIndex; }
    D3D12_CPU_DESCRIPTOR_HANDLE Cpu() const { return m_cpu; }

private:
    CpuDescriptorHeap* m_heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpu{};
    uint32_t m_index = CpuDescriptorHeap::kInvalidIndex;
};

// Depth-stencil view, allocated from a CPU-only DSV heap.
class DSVDescriptor {
public:
    DSVDescriptor() = default;
    ~DSVDescriptor();

    DSVDescriptor(const DSVDescriptor&) = delete;
    DSVDescriptor& operator=(const DSVDescriptor&) = delete;

    bool Create(
        ID3D12Device* device,
        CpuDescriptorHeap& heap,
        ID3D12Resource* resource,
        DXGI_FORMAT viewFormat);
    void Release();

    bool IsValid() const { return m_index != CpuDescriptorHeap::kInvalidIndex; }
    D3D12_CPU_DESCRIPTOR_HANDLE Cpu() const { return m_cpu; }

private:
    CpuDescriptorHeap* m_heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpu{};
    uint32_t m_index = CpuDescriptorHeap::kInvalidIndex;
};

} // namespace summer::gfx
