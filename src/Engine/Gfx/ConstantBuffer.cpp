#include "Gfx/ConstantBuffer.h"
#include "Gfx/Device.h"
#include "Gfx/Helpers.h"

#include <cstring>

namespace summer::gfx {

namespace {
constexpr uint32_t Align(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}
} // namespace

ConstantBuffer::~ConstantBuffer() {
    Shutdown();
}

bool ConstantBuffer::Initialize(Device& device, uint32_t elementSize, uint32_t count) {
    Shutdown();
    if (elementSize == 0 || count == 0) {
        return false;
    }

    m_alignedSize = Align(elementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    m_count = count;

    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = static_cast<UINT64>(m_alignedSize) * m_count;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (!CheckHRESULT(
            device.GetD3D12Device()->CreateCommittedResource(
                &uploadHeap,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_resource)),
            "CreateCommittedResource constant buffer")) {
        return false;
    }

    D3D12_RANGE readRange{0, 0};
    if (!CheckHRESULT(
            m_resource->Map(0, &readRange, reinterpret_cast<void**>(&m_mapped)),
            "Map constant buffer")) {
        Shutdown();
        return false;
    }

    return true;
}

void ConstantBuffer::Shutdown() {
    if (m_resource && m_mapped) {
        m_resource->Unmap(0, nullptr);
        m_mapped = nullptr;
    }
    if (m_resource) {
        m_resource->Release();
        m_resource = nullptr;
    }
    m_alignedSize = 0;
    m_count = 0;
}

void ConstantBuffer::Update(uint32_t index, const void* data, uint32_t size) {
    if (!m_mapped || index >= m_count || size > m_alignedSize) {
        return;
    }
    std::memcpy(m_mapped + static_cast<size_t>(index) * m_alignedSize, data, size);
}

D3D12_GPU_VIRTUAL_ADDRESS ConstantBuffer::GetGpuAddress(uint32_t index) const {
    if (!m_resource || index >= m_count) {
        return 0;
    }
    return m_resource->GetGPUVirtualAddress() + static_cast<UINT64>(index) * m_alignedSize;
}

} // namespace summer::gfx
