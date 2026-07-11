#pragma once

#include <cstdint>
#include <d3d12.h>
#include <vector>

namespace summer::gfx {

// A shader-visible CBV/SRV/UAV descriptor heap with a simple free-list allocator.
//
// DX12 only allows one shader-visible CBV/SRV/UAV heap to be bound at a time, so ImGui and
// all engine textures share this single heap. Hand out slots via Allocate()/Free(), or the
// raw variants used by the ImGui backend's alloc/free callbacks.
class GpuDescriptorHeap {
public:
    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    struct Handle {
        D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        uint32_t index = kInvalidIndex;

        bool IsValid() const { return index != kInvalidIndex; }
    };

    GpuDescriptorHeap() = default;
    ~GpuDescriptorHeap();

    GpuDescriptorHeap(const GpuDescriptorHeap&) = delete;
    GpuDescriptorHeap& operator=(const GpuDescriptorHeap&) = delete;

    bool Initialize(ID3D12Device* device, uint32_t capacity);
    void Shutdown();

    Handle Allocate();
    void Free(const Handle& handle);

    // Handle-less variants for the ImGui DX12 backend callbacks.
    void AllocateRaw(D3D12_CPU_DESCRIPTOR_HANDLE* outCpu, D3D12_GPU_DESCRIPTOR_HANDLE* outGpu);
    void FreeRaw(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu);

    ID3D12DescriptorHeap* Heap() const { return m_heap; }

private:
    Handle HandleForIndex(uint32_t index) const;

    ID3D12DescriptorHeap* m_heap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
    uint32_t m_increment = 0;
    uint32_t m_capacity = 0;
    std::vector<uint32_t> m_freeIndices;
};

} // namespace summer::gfx
