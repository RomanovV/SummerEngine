#include "Gfx/CpuDescriptorHeap.h"
#include "Gfx/Helpers.h"

#include <spdlog/spdlog.h>

namespace summer::gfx {

CpuDescriptorHeap::~CpuDescriptorHeap() {
    Shutdown();
}

bool CpuDescriptorHeap::Initialize(
    ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity) {
    if (!device || capacity == 0) {
        return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = capacity;
    desc.Type = type;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // CPU-only (RTV/DSV are never shader-visible)

    if (!CheckHRESULT(
            device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)),
            "CreateDescriptorHeap (CPU-only)")) {
        return false;
    }

    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_increment = device->GetDescriptorHandleIncrementSize(type);
    m_capacity = capacity;

    // Descending free list so the first Allocate() hands out index 0.
    m_freeIndices.clear();
    m_freeIndices.reserve(capacity);
    for (uint32_t i = capacity; i-- > 0;) {
        m_freeIndices.push_back(i);
    }
    return true;
}

void CpuDescriptorHeap::Shutdown() {
    m_freeIndices.clear();
    m_capacity = 0;
    if (m_heap) {
        m_heap->Release();
        m_heap = nullptr;
    }
}

uint32_t CpuDescriptorHeap::Allocate() {
    if (m_freeIndices.empty()) {
        spdlog::error("CpuDescriptorHeap is full (capacity {})", m_capacity);
        return kInvalidIndex;
    }
    const uint32_t index = m_freeIndices.back();
    m_freeIndices.pop_back();
    return index;
}

void CpuDescriptorHeap::Free(uint32_t index) {
    if (index >= m_capacity) {
        return;
    }
    m_freeIndices.push_back(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHeap::HandleForIndex(uint32_t index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cpuStart;
    handle.ptr += static_cast<SIZE_T>(index) * m_increment;
    return handle;
}

} // namespace summer::gfx
