#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

// A CPU-only (non-shader-visible) descriptor heap with a simple free-list allocator.
//
// Acts as a global pool for RTV or DSV descriptors (one instance per heap type), mirroring how
// GpuDescriptorHeap pools shader-visible CBV/SRV/UAV descriptors. RenderTarget/DepthStencil borrow
// individual slots from these pools instead of each creating its own tiny heap.
class CpuDescriptorHeap {
public:
    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    CpuDescriptorHeap() = default;
    ~CpuDescriptorHeap();

    CpuDescriptorHeap(const CpuDescriptorHeap&) = delete;
    CpuDescriptorHeap& operator=(const CpuDescriptorHeap&) = delete;

    bool Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity);
    void Shutdown();

    // Reserves one slot; returns kInvalidIndex when the heap is full.
    uint32_t Allocate();
    void Free(uint32_t index);

    D3D12_CPU_DESCRIPTOR_HANDLE HandleForIndex(uint32_t index) const;

private:
    ID3D12DescriptorHeap* m_heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    uint32_t m_increment = 0;
    uint32_t m_capacity = 0;
    std::vector<uint32_t> m_freeIndices;
};

} // namespace summer::gfx
