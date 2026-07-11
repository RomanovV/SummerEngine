#include "Gfx/GpuDescriptorHeap.h"
#include "Gfx/Helpers.h"

#include <spdlog/spdlog.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace summer::gfx {

GpuDescriptorHeap::~GpuDescriptorHeap() {
    Shutdown();
}

bool GpuDescriptorHeap::Initialize(ID3D12Device* device, uint32_t capacity) {
    if (!device || capacity == 0) {
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = capacity;
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> heap;
    if (!CheckHRESULT(
            device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)),
            "CreateDescriptorHeap (shared CBV/SRV/UAV)")) {
        return false;
    }

    heap->AddRef();
    m_heap = heap.Get();
    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_capacity = capacity;

    // Allocate from the front by popping the back of a descending free list.
    m_freeIndices.clear();
    m_freeIndices.reserve(capacity);
    for (uint32_t i = capacity; i-- > 0;) {
        m_freeIndices.push_back(i);
    }

    return true;
}

void GpuDescriptorHeap::Shutdown() {
    m_freeIndices.clear();
    m_capacity = 0;
    if (m_heap) {
        m_heap->Release();
        m_heap = nullptr;
    }
}

GpuDescriptorHeap::Handle GpuDescriptorHeap::HandleForIndex(uint32_t index) const {
    Handle handle;
    handle.index = index;
    handle.cpu.ptr = m_cpuStart.ptr + static_cast<SIZE_T>(index) * m_increment;
    handle.gpu.ptr = m_gpuStart.ptr + static_cast<UINT64>(index) * m_increment;
    return handle;
}

GpuDescriptorHeap::Handle GpuDescriptorHeap::Allocate() {
    if (m_freeIndices.empty()) {
        spdlog::error("GpuDescriptorHeap is full (capacity {})", m_capacity);
        return {};
    }

    const uint32_t index = m_freeIndices.back();
    m_freeIndices.pop_back();
    return HandleForIndex(index);
}

void GpuDescriptorHeap::Free(const Handle& handle) {
    if (!handle.IsValid() || handle.index >= m_capacity) {
        return;
    }
    m_freeIndices.push_back(handle.index);
}

void GpuDescriptorHeap::AllocateRaw(
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
    D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
    const Handle handle = Allocate();
    if (outCpu) {
        *outCpu = handle.cpu;
    }
    if (outGpu) {
        *outGpu = handle.gpu;
    }
}

void GpuDescriptorHeap::FreeRaw(D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu) {
    (void)gpu;
    if (m_increment == 0) {
        return;
    }
    const uint32_t index = static_cast<uint32_t>((cpu.ptr - m_cpuStart.ptr) / m_increment);
    Handle handle;
    handle.index = index;
    Free(handle);
}

} // namespace summer::gfx
